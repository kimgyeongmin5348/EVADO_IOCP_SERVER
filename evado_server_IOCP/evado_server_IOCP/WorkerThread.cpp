#include "workerthread.h"
#include "Common.h" // 공통 헤더 파일 포함
#include "Item.h"
#include "ItemManager.h"
#include "Monster.h"
#include "MonsterManager.h"


// 전역 변수 초기화
HANDLE g_hIOCP;
std::atomic<long long> g_client_counter = 0;
std::unordered_map<long long, SESSION*> g_sessions;
std::mutex g_session_mutex;
SOCKET g_listen_socket = INVALID_SOCKET;
std::atomic<int> g_new_id = 0;

//Item
ItemManager g_item_manager;



void safe_remove_session(long long id) {
	SESSION* target = nullptr;
	{
		std::lock_guard<std::mutex> lock(g_session_mutex);
		auto it = g_sessions.find(id);
		if (it == g_sessions.end()) return;
		target = it->second;
		g_sessions.erase(it); // 맵에서 먼저 제거
	}

	// 뮤텍스 밖에서 소켓 닫기 및 메모리 해제
	if (target) {
		if (target->_c_socket != INVALID_SOCKET) {
			closesocket(target->_c_socket);
			target->_c_socket = INVALID_SOCKET;
		}
		delete target;
	}
}

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
	_remained = 0;
	do_recv();
}

// leave 관련 구현
SESSION::~SESSION()
{
	if (_c_socket != INVALID_SOCKET) {
		shutdown(_c_socket, SD_SEND);
		closesocket(_c_socket);
		_c_socket = INVALID_SOCKET;
	}

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
			safe_remove_session(_id); // 직접 삭제 대신 안전 제거 함수 호출
			return; // delete this 제거!
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
			safe_remove_session(_id); // 직접 삭제 대신 안전 제거 함수 호출
			delete o;
		}
	}
}

void SESSION::send_player_info_packet()
{
	sc_packet_user_info p{};
	p.size = sizeof(p);
	p.type = SC_P_USER_INFO;
	p.id = _id;
	p.position = _position;
	p.look = _look;
	p.right = _right;
	p.animState = _animState;
	//p.hp = 100;
	do_send(&p);
}

void BroadcastToAll(void* pkt, long long exclude_id = -1) {
	std::vector<SESSION*> sessions;
	{
		std::lock_guard<std::mutex> lock(g_session_mutex);
		for (auto& [id, session] : g_sessions) {
			if (session->_c_socket != INVALID_SOCKET && id != exclude_id)
				sessions.push_back(session);
		}
	}

	for (auto session : sessions) {
		auto packet_copy = new char[256]; // 적절한 크기 할당
		memcpy(packet_copy, pkt, reinterpret_cast<unsigned char*>(pkt)[0]);
		session->do_send(packet_copy);
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
		std::cout << "[서버] " << _id << "번 클라이언트 로그인: " << _name << std::endl;

		// 1. 자신의 정보 전송
		send_player_info_packet();

		// 2. 기존 유저 정보 전송
		std::vector<sc_packet_enter> existing_users;
		{
			std::lock_guard<std::mutex> lock(g_session_mutex);
			for (auto& [ex_id, ex_session] : g_sessions) {
				if (ex_id == _id) continue;

				sc_packet_enter pkt;
				pkt.size = sizeof(pkt);
				pkt.type = SC_P_ENTER;
				pkt.id = ex_id;
				pkt.position = ex_session->_position;
				pkt.look = ex_session->_look;
				pkt.right = ex_session->_right;
				pkt.animState = ex_session->GetAnimationState();
				existing_users.push_back(pkt);
			}
		}
		for (auto& pkt : existing_users) {
			do_send(&pkt);
		}

		// 2. 기존 아이템 정보 전송
		auto items = g_item_manager.GetAllItems();
		for (auto* item : items) {
			sc_packet_item_spawn pkt;
			pkt.size = sizeof(pkt);
			pkt.type = SC_P_ITEM_SPAWN;
			pkt.item_id = item->GetID();
			pkt.position = item->GetPosition();
			pkt.item_type = item->GetType();
			do_send(&pkt);
		}

		// 3. 신규 유저 정보 브로드캐스트
		sc_packet_enter new_user_pkt;
		new_user_pkt.size = sizeof(new_user_pkt);
		new_user_pkt.type = SC_P_ENTER;
		new_user_pkt.id = _id;
		new_user_pkt.position = _position;
		new_user_pkt.look = _look;
		new_user_pkt.right = _right;
		new_user_pkt.animState = _animState;
		BroadcastToAll(&new_user_pkt, _id); // 자신 제외 전체 전송

		// 4. Monster 정보 전송
		auto monsters = MonsterManager::GetInstance().GetAllMonsters();
		for (auto& [monster_id, monster] : monsters) {
			sc_packet_monster_spawn pkt;
			pkt.size = sizeof(pkt);
			pkt.type = SC_P_MONSTER_SPAWN;
			pkt.monsterID = monster->GetSpiderID();
			pkt.position = monster->GetSpiderPosition();
			pkt.state = monster->GetSpiderAnimaitionState();
			do_send(&pkt); // 새 클라이언트에게만 전송
		}
	
		break;
	}
	case CS_P_MOVE: { 
		cs_packet_move* packet = reinterpret_cast<cs_packet_move*>(p);
		_position = packet->position;
		_look = packet->look;
		_right = packet->right;
		_animState = packet->animState;

		//std::cout << "[서버] " << _id << "번 클라이언트 위치 수신: (" << _position.x << ", " << _position.y << ", " << _position.z << ", "
		//	<< _look.x << ", " << _look.y << ", " << _look.z << ", "
		//	<< _right.x << ", " << _right.y << ", " << _right.z << ", " << static_cast<int>(_animState) << ")\n";

		sc_packet_move mp;
		mp.size = sizeof(mp);
		mp.type = SC_P_MOVE;
		mp.id = _id;
		mp.position = _position;
		mp.look = _look;
		mp.right = _right;
		mp.animState = _animState;

		std::cout << "[서버] " << _id << "번 클라이언트 위치 수신: (" << _position.x << ", " << _position.y << ", " << _position.z << ", "
			<< _look.x << ", " << _look.y << ", " << _look.z << ", "
			<< _right.x << ", " << _right.y << ", " << _right.z << ", " << static_cast<int>(_animState) << ")\n";
		std::cout << "[서버] 브로드캐스트 시작 - 대상 수: " << g_sessions.size() - 1 << "\n";

		BroadcastToAll(&mp, _id);
		break;
	}

	case CS_P_ITEM_PICKUP: 
	{
		cs_packet_item_pickup* packet = reinterpret_cast<cs_packet_item_pickup*>(p);
		HandleItemPickup(packet->item_id);
		break;
	}
	
	case CS_P_ITEM_MOVE: 
	{
		cs_packet_item_move* packet = reinterpret_cast<cs_packet_item_move*>(p);
		Item* item = g_item_manager.GetItem(packet->item_id);
		if (item && item->GetHolder() == _id) {
			item->SetPosition(packet->position);

			// 모든 클라이언트에 위치 업데이트
			sc_packet_item_move move_pkt;
			move_pkt.size = sizeof(move_pkt);
			move_pkt.type = SC_P_ITEM_MOVE;
			move_pkt.item_id = packet->item_id;
			move_pkt.position = packet->position;
			move_pkt.holder_id = _id;

			BroadcastToAll(&move_pkt);
		}
		break;
	}

	default:
		std::cout << "[경고] 잘못된 패킷 타입: " << (int)packet_type << "\n";
		safe_remove_session(_id); // 연결 종료
		break;
	}
}

// 아이템 획득 처리
void SESSION::HandleItemPickup(long long item_id) {
	Item* item = g_item_manager.GetItem(item_id);
	if (!item || item->GetHolder() != 0) {
		std::cout << "[서버] " << _id << "번 플레이어 아이템 획득 실패: "
			<< (item ? "이미 소유중" : "존재하지 않음") << "\n";
		return;
	}

	item->SetHolder(_id);

	sc_packet_item_despawn pkt;
	pkt.size = sizeof(pkt);
	pkt.type = SC_P_ITEM_DESPAWN;
	pkt.item_id = item_id;

	BroadcastToAll(&pkt);
	
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

// 아이템 생성 처리
void SpawnItemToAll(long long id, XMFLOAT3 pos, int item_type) {
	sc_packet_item_spawn pkt;
	pkt.size = sizeof(pkt);
	pkt.type = SC_P_ITEM_SPAWN;
	pkt.item_id = id;
	pkt.position = pos;
	pkt.item_type = item_type; // 새 필드 추가

	BroadcastToAll(&pkt); // exclude_id 기본값 0 (모두에게 전송)
}

void TestSpawnMultipleItems() {
	for (int i = 0; i < 5; ++i) {
		XMFLOAT3 pos = { 10.0f + i * 2, 0.0f, 5.0f + i * 3 };
		int item_type = (i % 3) + 1;
		long long item_id = 20000 + i;
		g_item_manager.SpawnItem(item_id, pos, item_type);
		SpawnItemToAll(item_id, pos, item_type);
	}
}

// 몬스터 생성
void GameLoopThread() {
	auto last_time = std::chrono::steady_clock::now();
	while (true) {
		auto now = std::chrono::steady_clock::now();
		float delta_time =
			std::chrono::duration_cast<std::chrono::milliseconds>(now - last_time).count() / 1000.0f;
		last_time = now;

		// 플레이어 위치 수집
		std::vector<XMFLOAT3> playerPositions;
		{
			std::lock_guard<std::mutex> lock(g_session_mutex);
			for (auto& [id, session] : g_sessions) {
				playerPositions.push_back(session->_position);
			}
		}

		// 몬스터 업데이트
		MonsterManager::GetInstance().UpdateAllMonsters(delta_time, playerPositions);

		std::this_thread::sleep_for(std::chrono::milliseconds(50));
	}
}

void InitializeMonsters() {
	MonsterManager::GetInstance().SpawnMonster(10001, XMFLOAT3{ 10.f, 0.f, 10.f },
		static_cast<uint8_t>(MonsterAnimationState::IDLE));
	MonsterManager::GetInstance().SpawnMonster(10002, XMFLOAT3{ 0.f, 0.f, 0.f },
		static_cast<uint8_t>(MonsterAnimationState::IDLE));
}

// Worker Thread 핸들러
void WorkerThread() {
	while (true) {
		DWORD io_size;
		WSAOVERLAPPED* o;
		ULONG_PTR key;
		BOOL ret = GetQueuedCompletionStatus(g_hIOCP, &io_size, &key, &o, INFINITE);
		EXP_OVER* eo = reinterpret_cast<EXP_OVER*>(o);

		if (FALSE == ret || (0 == io_size && (eo->_io_op == IO_RECV || eo->_io_op == IO_SEND))) {
			if (eo->_io_op == IO_RECV) {
				safe_remove_session(key);
			}
			delete eo;
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
		{
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
				safe_remove_session(key);
				delete eo;  // EXP_OVER 객체 정리
				continue;
			}

			// 2. 세션 작업 (락이 해제된 상태에서 진행)
			SESSION& user = *pUser;  // 역참조

			unsigned char* p = eo->_buffer;
			int data_size = io_size + user._remained;

			while (p < eo->_buffer + data_size) {
				if (data_size < 2) break; // 최소 패킷 크기(헤더 2바이트) 확인
				unsigned char packet_size = p[0];

				// 패킷 크기 검증 (헤더 포함 전체 크기)
				if (packet_size < sizeof(unsigned char) ||
					packet_size > MAX_PACKET_SIZE ||
					(p + packet_size) > (eo->_buffer + data_size)) {
					std::cerr << "[오류] 잘못된 패킷 크기: " << (int)packet_size << "\n";
					break;
				}

				user.process_packet(p);
				p += packet_size;
			}

			if (p < eo->_buffer + data_size) {
				user._remained = static_cast<unsigned char>(eo->_buffer + data_size - p);
				memcpy(p, eo->_buffer, user._remained);
			}
			else
				user._remained = 0;
			pUser->do_recv();
			break;
		}
		}
	}
}

