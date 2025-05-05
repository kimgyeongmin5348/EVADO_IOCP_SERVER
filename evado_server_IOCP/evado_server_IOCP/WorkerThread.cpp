#include "workerthread.h"
#include "Common.h" // 공통 헤더 파일 포함


// 전역 변수 초기화
HANDLE g_hIOCP;
std::atomic<long long> g_client_counter = 0;
std::unordered_map<long long, SESSION*> g_sessions;
std::mutex g_session_mutex;
SOCKET g_listen_socket = INVALID_SOCKET;
EXP_OVER g_accept_over{ IO_ACCEPT };

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

	std::lock_guard<std::mutex> lock(g_session_mutex);
	g_sessions[_id] = this;
	std::cout << "[서버] " << _id << "번 클라이언트 접속 성공\n";
	do_recv();
}

// leave 관련 구현
SESSION::~SESSION()
{
	sc_packet_leave lp;
	lp.size = sizeof(lp);
	lp.type = SC_P_LEAVE;
	lp.id = _id;
	for (auto& u : g_sessions) {
		if (_id != u.first)
			u.second->do_send(&lp);
	}
	closesocket(_c_socket);
}

void SESSION::do_recv()
{
	DWORD recv_flag = 0;
	ZeroMemory(&_recv_over._over, sizeof(_recv_over._over));
	_recv_over._wsabuf[0].buf = reinterpret_cast<CHAR*>(_recv_over._buffer + _remained);
	_recv_over._wsabuf[0].len = sizeof(_recv_over._buffer) - _remained;

	auto ret = WSARecv(_c_socket, _recv_over._wsabuf, 1, NULL,
		&recv_flag, &_recv_over._over, NULL);
	if (0 != ret) {
		auto err_no = WSAGetLastError();
		if (WSA_IO_PENDING != err_no) {
			print_error_message(err_no);
			exit(-1);
		}
	}
	std::cout << "[서버] " << _id << "번 소켓 수신 대기 시작\n";
}

void SESSION::do_send(void* buff)
{
	EXP_OVER* o = new EXP_OVER(IO_SEND);
	const unsigned char packet_size = reinterpret_cast<unsigned char*>(buff)[0];
	memcpy_s(o->_buffer, sizeof(o->_buffer), buff, packet_size);
	o->_wsabuf[0].len = packet_size;
	DWORD size_sent;
	WSASend(_c_socket, o->_wsabuf, 1, &size_sent, 0, &(o->_over), NULL);
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
	//strncpy_s(p.name, _name.c_str(), _TRUNCATE);
	p.position = _position;
	do_send(&p);
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

		// 서버 위치 정보 업데이트 추가
		_position = packet->position;  // 클라이언트에서 전송한 위치 반영

		

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

void do_accept(SOCKET s_socket, EXP_OVER* accept_over)
{
	SOCKET c_socket = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, 0, 0, WSA_FLAG_OVERLAPPED);;
	accept_over->_accept_socket = c_socket;
	AcceptEx(s_socket, c_socket, accept_over->_buffer, 0, sizeof(SOCKADDR_IN) + 16, sizeof(SOCKADDR_IN) + 16, NULL, &accept_over->_over);
}



//EXP_OVER g_accept_over{ IO_ACCEPT };

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
			
			long long new_id = g_new_id++;
			SOCKET client_socket = eo->_accept_socket;

			CreateIoCompletionPort(reinterpret_cast<HANDLE>(client_socket), g_hIOCP, new_id, 0);
			g_sessions.try_emplace(new_id, new SESSION(new_id, eo->_accept_socket));
			
			do_accept(g_listen_socket, &g_accept_over);
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
					return;
				}
				pUser = it->second;  // 포인터 추출
			}  // 여기서 락 자동 해제

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

	