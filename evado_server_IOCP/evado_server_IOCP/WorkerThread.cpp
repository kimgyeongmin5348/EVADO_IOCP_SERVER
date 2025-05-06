#include "workerthread.h"
#include "Common.h" // 공통 헤더 파일 포함


// 전역 변수 초기화
HANDLE g_hIOCP;
std::atomic<long long> g_client_counter = 0;
std::unordered_map<long long, SESSION*> g_sessions;
std::mutex g_session_mutex;
SOCKET g_listen_socket = INVALID_SOCKET;
std::atomic<int> g_new_id = 0;

// EXP_OVER 구현
EXP_OVER::EXP_OVER(IO_OP op) : _io_op(op)
{
	ZeroMemory(&_over, sizeof(_over));
	_wsabuf[0].buf = reinterpret_cast<CHAR*>(_buffer);
	_wsabuf[0].len = sizeof(_buffer);
}

SESSION::SESSION()
{
	std::cout << "DEFAULT SESSION CONSTRUCTOR CALLED!!\n";
	exit(-1);
}

// SESSION 구현
SESSION::SESSION(long long session_id, SOCKET s) : _id(session_id), _c_socket(s), _recv_over(IO_RECV)
{
	// 소켓 옵션 추가 (Keep-Alive 설정)
	int opt = 1;
	setsockopt(_c_socket, SOL_SOCKET, SO_KEEPALIVE, (char*)&opt, sizeof(opt));

	// Nagle 알고리즘 비활성화 (실시간 통신 필수)
	setsockopt(_c_socket, IPPROTO_TCP, TCP_NODELAY, (char*)&opt, sizeof(opt));

	{
		std::lock_guard<std::mutex> lock(g_session_mutex);
		g_sessions[_id] = this;
		std::cout << "[서버] 세션 추가 완료: ID=" << _id << ", 현재 접속자 수: " << g_sessions.size() << "\n";
	}
	do_recv();
}

// leave 관련 구현
SESSION::~SESSION()
{
	LINGER linger_opt = { 1, 0 }; // 즉시 닫기
	setsockopt(_c_socket, SOL_SOCKET, SO_LINGER, (char*)&linger_opt, sizeof(linger_opt));

	sc_packet_leave lp{};
	lp.size = sizeof(lp);
	lp.type = SC_P_LEAVE;
	lp.id = _id;
	// 다른 세션 목록 임시 복사
	std::vector<SESSION*> other_sessions;
	{
		std::lock_guard<std::mutex> lock(g_session_mutex);
		for (auto& u : g_sessions) {
			if (_id != u.first && u.second->_c_socket != INVALID_SOCKET)
				other_sessions.push_back(u.second);
		}
	}

	// 뮤텍스 외부에서 전송 수행
	for (auto* session : other_sessions) {
		session->do_send(&lp);
	}

	closesocket(_c_socket);
	_c_socket = INVALID_SOCKET;
}

void SESSION::do_recv() {
	DWORD recv_flag = 0;
	ZeroMemory(&_recv_over._over, sizeof(_recv_over._over));
	_recv_over._wsabuf[0].buf = reinterpret_cast<CHAR*>(_recv_over._buffer + _remained);
	_recv_over._wsabuf[0].len = sizeof(_recv_over._buffer) - _remained;

	auto ret = WSARecv(_c_socket, _recv_over._wsabuf, 1, NULL, &recv_flag, &_recv_over._over, NULL);
	if (0 != ret) {
		auto err_no = WSAGetLastError();
		if (WSA_IO_PENDING != err_no) {
			std::cout << "[오류] " << _id << "번 클라이언트 연결 종료. 코드: " << err_no << "\n";
			{
				std::lock_guard<std::mutex> lock(g_session_mutex);
				g_sessions.erase(_id);
			}
			closesocket(_c_socket);
			_c_socket = INVALID_SOCKET;
			delete this;
			return;
		}
	}
	std::cout << "[서버] " << _id << "번 소켓 수신 대기 시작\n";
}


void SESSION::do_send(void* buff)
{
	if (_c_socket == INVALID_SOCKET) return;
	EXP_OVER* o = new EXP_OVER(IO_SEND);
	const unsigned char packet_size = reinterpret_cast<unsigned char*>(buff)[0];
	memcpy_s(o->_buffer, sizeof(o->_buffer), buff, packet_size);
	o->_wsabuf[0].len = packet_size;

	int ret = WSASend(_c_socket, o->_wsabuf, 1, NULL, 0, &o->_over, NULL);
	if (SOCKET_ERROR == ret) {
		int error = WSAGetLastError();
		if (error != WSA_IO_PENDING) {
			std::cout << "[오류] 전송 실패: " << error << std::endl;
			closesocket(_c_socket);
			_c_socket = INVALID_SOCKET;
			delete o;

			// 세션 제거 및 메모리 해제
			std::lock_guard<std::mutex> lock(g_session_mutex);
			auto it = g_sessions.find(_id);
			if (it != g_sessions.end()) {
				g_sessions.erase(it);
				delete this; // 현재 세션 삭제
			}
			return;
		}
	}
}

void SESSION::send_player_info_packet()
{
	sc_packet_user_info p{};
	p.size = sizeof(p);
	p.type = SC_P_USER_INFO;
	p.id = _id;
	//strncpy_s(p.name, _name.c_str(), _TRUNCATE);
	p.position = _position;
	//p.look = _look;
	//p.right = _right;
	//p.hp = 100;
	do_send(&p);
}

void SESSION::broadcast_move_packet() {
	sc_packet_move p;
	p.size = sizeof(p);
	p.type = SC_P_MOVE;
	p.id = _id;
	p.position = _position;

	std::lock_guard<std::mutex> lock(g_session_mutex);
	for (auto& [id, session] : g_sessions) {
		if (id == _id) continue;
		if (session->_c_socket == INVALID_SOCKET) continue;
		session->do_send(&p);
		std::cout << "[서버] " << id << "번 전송: (" << p.position.x << "," << p.position.y << "," << p.position.z << ")\n";
	}
}

void SESSION::process_packet(unsigned char* p)
{
	std::cout << "[서버] 패킷 처리 시작 - 크기: " << (int)p[0] << ", 타입: " << (int)p[1] << "\n";
	const unsigned char packet_type = p[1];
	switch (packet_type) {
	case CS_P_LOGIN: 
	{
		cs_packet_login* packet = reinterpret_cast<cs_packet_login*>(p);
		_name = packet->name;
		_position = packet->position;
		std::cout << "[서버] 로그인 요청 수신: " << _name << " 위치(" << _position.x << "," << _position.y << "," << _position.z << ")\n";

		std::cout << "[서버] " << _id << "번 클라이언트 로그인: " << _name << std::endl;

		// 1. 자신의 정보 전송
		send_player_info_packet();


		// 2. 기존 클라이언트들에게 신규 접속 알림
		sc_packet_enter new_user_pkt;
		new_user_pkt.size = sizeof(new_user_pkt);
		new_user_pkt.type = SC_P_ENTER;
		new_user_pkt.id = _id;
		strcpy_s(new_user_pkt.name, sizeof(new_user_pkt.name), _name.c_str()); // 안전한 복사
		new_user_pkt.o_type = 0;
		new_user_pkt.position = _position;

		// 3. 신규 클라이언트에게 기존 유저 정보 전송
		{
			std::lock_guard<std::mutex> lock(g_session_mutex);
			for (auto& [ex_id, ex_session] : g_sessions) {
				if (ex_id == _id) continue;

				// 기존 유저 정보 패킷 생성
				sc_packet_enter existing_user_pkt;
				existing_user_pkt.size = sizeof(existing_user_pkt);
				existing_user_pkt.type = SC_P_ENTER;
				existing_user_pkt.id = ex_id;
				strcpy_s(existing_user_pkt.name, sizeof(existing_user_pkt.name), ex_session->_name.c_str());
				existing_user_pkt.position = ex_session->_position;

				// 신규 클라이언트에 전송
				do_send(&existing_user_pkt);
			}
		}
		{
			std::lock_guard<std::mutex> lock(g_session_mutex);
			for (auto& [ex_id, ex_session] : g_sessions) {
				if (ex_id != _id) {
					ex_session->do_send(&new_user_pkt);
				}
			}
		}		
		break;
	}

	case CS_P_MOVE: { // 이부분을 클라이언트랑 이야기 하면서 고쳐봐야 겠음. ( 난 서버에서 계산 해서 보내는것, 클라쪽은 그냥 좌표만 받자라는 생각)
		cs_packet_move* packet = reinterpret_cast<cs_packet_move*>(p);

		std::cout << "[서버] " << _id << "번 클라이언트 위치 수신: (" << _position.x << ", " << _position.y << ", " << _position.z << ")\n";


		sc_packet_move mp;
		mp.size = sizeof(mp);
		mp.type = SC_P_MOVE;
		mp.id = _id;
		mp.position = _position;
		std::cout << "[서버] 브로드캐스트 시작 - 대상 수: " << g_sessions.size() - 1 << "\n";

		// 모든 클라이언트에게 브로드캐스트
		{
			std::lock_guard<std::mutex> lock(g_session_mutex);
			for (auto& [id, session] : g_sessions) {
				if (id != _id) {
					session->do_send(&mp);
					std::cout << "[서버] " << id << "번 전송: (" << mp.position.x << "," << mp.position.y << "," << mp.position.z << ")\n";
				}
			}
		}
		break;
	}
	default:
		std::cout << "Error Invalid Packet Type\n";
		exit(-1);
	}
}

void print_error_message(int s_err)
{
	WCHAR* lpMsgBuf;
	FormatMessage(
		FORMAT_MESSAGE_ALLOCATE_BUFFER |
		FORMAT_MESSAGE_FROM_SYSTEM,
		NULL, s_err,
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		(LPTSTR)&lpMsgBuf, 0, NULL);
	std::wcout << lpMsgBuf << std::endl;
	while (true); // 디버깅 용
	LocalFree(lpMsgBuf);
}

void do_accept(SOCKET s_socket) {
	EXP_OVER* accept_over = new EXP_OVER(IO_ACCEPT);
	SOCKET c_socket = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, 0, 0, WSA_FLAG_OVERLAPPED);

	// 소켓 옵션 설정 (Nagle 알고리즘 비활성화)
	int opt = 1;
	setsockopt(c_socket, IPPROTO_TCP, TCP_NODELAY, (char*)&opt, sizeof(opt));

	accept_over->_accept_socket = c_socket;

	// AcceptEx 호출
	if (!AcceptEx(s_socket, c_socket, accept_over->_buffer, 0,
		sizeof(SOCKADDR_IN) + 16, sizeof(SOCKADDR_IN) + 16,
		NULL, &accept_over->_over))
	{
		int err = WSAGetLastError();
		if (err != ERROR_IO_PENDING) {
			print_error_message(err);
			delete accept_over;
			closesocket(c_socket);
		}
	}
}


// Worker Thread 핸들러
void WorkerThread() {
	while (true) {
		DWORD io_size;
		WSAOVERLAPPED* o;
		ULONG_PTR key;
		BOOL ret = GetQueuedCompletionStatus(g_hIOCP, &io_size, &key, &o, INFINITE);
		EXP_OVER* eo = reinterpret_cast<EXP_OVER*>(o);

		if (FALSE == ret) {
			auto err_no = WSAGetLastError();
			print_error_message(err_no);
			if (g_sessions.count(key) != 0)
				g_sessions.erase(key);
			continue;
		}
		if ((eo->_io_op == IO_RECV || eo->_io_op == IO_SEND) && (0 == io_size)) {
			if (g_sessions.count(key) != 0)
				g_sessions.erase(key);
			continue;
		}

		switch (eo->_io_op)
		{
		case IO_ACCEPT:
		{
			
			long long new_id = g_new_id.fetch_add(1);
			SOCKET client_socket = eo->_accept_socket;

			// 1. 클라이언트 주소 정보 추출
			SOCKADDR_IN* client_addr = nullptr;
			SOCKADDR_IN* local_addr = nullptr;
			int remote_addr_len = sizeof(SOCKADDR_IN);
			int local_addr_len = sizeof(SOCKADDR_IN);

			GetAcceptExSockaddrs(
				eo->_buffer, 0,
				sizeof(SOCKADDR_IN) + 16, sizeof(SOCKADDR_IN) + 16,
				(SOCKADDR**)&local_addr, &local_addr_len,
				(SOCKADDR**)&client_addr, &remote_addr_len
			);

			// 2. IP 주소 문자열 변환
			char ip_str[INET_ADDRSTRLEN];
			inet_ntop(AF_INET, &(client_addr->sin_addr), ip_str, INET_ADDRSTRLEN);
			std::cout << "[서버] 새로운 클라이언트 접속: IP=" << ip_str
				<< ", 포트=" << ntohs(client_addr->sin_port)
				<< ", 할당 ID=" << new_id << "\n";

			// 3. IOCP에 소켓 등록
			CreateIoCompletionPort(reinterpret_cast<HANDLE>(client_socket), g_hIOCP, new_id, 0);

			// 4. 세션 생성
			new SESSION(new_id, client_socket);

			// 5. 다음 Accept 요청
			do_accept(g_listen_socket);

			// 6. 현재 OVERLAPPED 메모리 해제
			delete eo;
			break;

		}
		case IO_SEND:
			delete eo;
			break;
		case IO_RECV:
			// 1. 뮤텍스 락으로 세션 검색 (스레드 세이프)
			SESSION* pUser = nullptr;
			{
				std::lock_guard<std::mutex> lock(g_session_mutex);
				auto it = g_sessions.find(key);
				if (it == g_sessions.end()) {
					// 세션이 이미 제거된 경우
					delete eo;  // EXP_OVER 객체 정리
					continue;
				}
				pUser = it->second;  // 포인터 추출
			}

			if (FALSE == ret || 0 == io_size) {
				std::cout << "[서버] " << key << "번 클라이언트 연결 종료\n";

				std::lock_guard<std::mutex> lock(g_session_mutex);
				if (g_sessions.count(key)) {
					SESSION* session = g_sessions[key];
					closesocket(session->_c_socket);  // 소켓 닫기
					delete session;                   // 세션 객체 메모리 해제
					g_sessions.erase(key);            // 맵에서 제거
				}
				delete eo;  // EXP_OVER 객체 정리
				continue;
			}

			// 2. 세션 작업 (락이 해제된 상태에서 진행)
			SESSION& user = *pUser;  // 역참조

			unsigned char* p = eo->_buffer;
			int data_size = io_size + user._remained;

			while (p < eo->_buffer + data_size) {
				unsigned char packet_size = *p;
				if (p + packet_size > eo->_buffer + data_size)
					break;
				user.process_packet(p);
				p = p + packet_size;
			}

			if (p < eo->_buffer + data_size) {
				user._remained = static_cast<unsigned char>(eo->_buffer + data_size - p);
				memcpy(p, eo->_buffer, user._remained);
			}
			else
				user._remained = 0;
			user.do_recv();
			break;
		}
	}
}

	