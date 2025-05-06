#include "workerthread.h"
#include "Common.h" // ���� ��� ���� ����


// ���� ���� �ʱ�ȭ
HANDLE g_hIOCP;
std::atomic<long long> g_client_counter = 0;
std::unordered_map<long long, SESSION*> g_sessions;
std::mutex g_session_mutex;
SOCKET g_listen_socket = INVALID_SOCKET;
std::atomic<int> g_new_id = 0;

// EXP_OVER ����
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

// SESSION ����
SESSION::SESSION(long long session_id, SOCKET s) : _id(session_id), _c_socket(s), _recv_over(IO_RECV)
{
	// ���� �ɼ� �߰� (Keep-Alive ����)
	int opt = 1;
	setsockopt(_c_socket, SOL_SOCKET, SO_KEEPALIVE, (char*)&opt, sizeof(opt));

	// Nagle �˰��� ��Ȱ��ȭ (�ǽð� ��� �ʼ�)
	setsockopt(_c_socket, IPPROTO_TCP, TCP_NODELAY, (char*)&opt, sizeof(opt));

	{
		std::lock_guard<std::mutex> lock(g_session_mutex);
		g_sessions[_id] = this;
		std::cout << "[����] ���� �߰� �Ϸ�: ID=" << _id << ", ���� ������ ��: " << g_sessions.size() << "\n";
	}
	do_recv();
}

// leave ���� ����
SESSION::~SESSION()
{
	LINGER linger_opt = { 1, 0 }; // ��� �ݱ�
	setsockopt(_c_socket, SOL_SOCKET, SO_LINGER, (char*)&linger_opt, sizeof(linger_opt));

	sc_packet_leave lp{};
	lp.size = sizeof(lp);
	lp.type = SC_P_LEAVE;
	lp.id = _id;
	// �ٸ� ���� ��� �ӽ� ����
	std::vector<SESSION*> other_sessions;
	{
		std::lock_guard<std::mutex> lock(g_session_mutex);
		for (auto& u : g_sessions) {
			if (_id != u.first && u.second->_c_socket != INVALID_SOCKET)
				other_sessions.push_back(u.second);
		}
	}

	// ���ؽ� �ܺο��� ���� ����
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
			std::cout << "[����] " << _id << "�� Ŭ���̾�Ʈ ���� ����. �ڵ�: " << err_no << "\n";
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
	std::cout << "[����] " << _id << "�� ���� ���� ��� ����\n";
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
			std::cout << "[����] ���� ����: " << error << std::endl;
			closesocket(_c_socket);
			_c_socket = INVALID_SOCKET;
			delete o;

			// ���� ���� �� �޸� ����
			std::lock_guard<std::mutex> lock(g_session_mutex);
			auto it = g_sessions.find(_id);
			if (it != g_sessions.end()) {
				g_sessions.erase(it);
				delete this; // ���� ���� ����
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
		std::cout << "[����] " << id << "�� ����: (" << p.position.x << "," << p.position.y << "," << p.position.z << ")\n";
	}
}

void SESSION::process_packet(unsigned char* p)
{
	std::cout << "[����] ��Ŷ ó�� ���� - ũ��: " << (int)p[0] << ", Ÿ��: " << (int)p[1] << "\n";
	const unsigned char packet_type = p[1];
	switch (packet_type) {
	case CS_P_LOGIN: 
	{
		cs_packet_login* packet = reinterpret_cast<cs_packet_login*>(p);
		_name = packet->name;
		_position = packet->position;
		std::cout << "[����] �α��� ��û ����: " << _name << " ��ġ(" << _position.x << "," << _position.y << "," << _position.z << ")\n";

		std::cout << "[����] " << _id << "�� Ŭ���̾�Ʈ �α���: " << _name << std::endl;

		// 1. �ڽ��� ���� ����
		send_player_info_packet();


		// 2. ���� Ŭ���̾�Ʈ�鿡�� �ű� ���� �˸�
		sc_packet_enter new_user_pkt;
		new_user_pkt.size = sizeof(new_user_pkt);
		new_user_pkt.type = SC_P_ENTER;
		new_user_pkt.id = _id;
		strcpy_s(new_user_pkt.name, sizeof(new_user_pkt.name), _name.c_str()); // ������ ����
		new_user_pkt.o_type = 0;
		new_user_pkt.position = _position;

		// 3. �ű� Ŭ���̾�Ʈ���� ���� ���� ���� ����
		{
			std::lock_guard<std::mutex> lock(g_session_mutex);
			for (auto& [ex_id, ex_session] : g_sessions) {
				if (ex_id == _id) continue;

				// ���� ���� ���� ��Ŷ ����
				sc_packet_enter existing_user_pkt;
				existing_user_pkt.size = sizeof(existing_user_pkt);
				existing_user_pkt.type = SC_P_ENTER;
				existing_user_pkt.id = ex_id;
				strcpy_s(existing_user_pkt.name, sizeof(existing_user_pkt.name), ex_session->_name.c_str());
				existing_user_pkt.position = ex_session->_position;

				// �ű� Ŭ���̾�Ʈ�� ����
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

	case CS_P_MOVE: { // �̺κ��� Ŭ���̾�Ʈ�� �̾߱� �ϸ鼭 ���ĺ��� ����. ( �� �������� ��� �ؼ� �����°�, Ŭ������ �׳� ��ǥ�� ���ڶ�� ����)
		cs_packet_move* packet = reinterpret_cast<cs_packet_move*>(p);

		std::cout << "[����] " << _id << "�� Ŭ���̾�Ʈ ��ġ ����: (" << _position.x << ", " << _position.y << ", " << _position.z << ")\n";


		sc_packet_move mp;
		mp.size = sizeof(mp);
		mp.type = SC_P_MOVE;
		mp.id = _id;
		mp.position = _position;
		std::cout << "[����] ��ε�ĳ��Ʈ ���� - ��� ��: " << g_sessions.size() - 1 << "\n";

		// ��� Ŭ���̾�Ʈ���� ��ε�ĳ��Ʈ
		{
			std::lock_guard<std::mutex> lock(g_session_mutex);
			for (auto& [id, session] : g_sessions) {
				if (id != _id) {
					session->do_send(&mp);
					std::cout << "[����] " << id << "�� ����: (" << mp.position.x << "," << mp.position.y << "," << mp.position.z << ")\n";
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
	while (true); // ����� ��
	LocalFree(lpMsgBuf);
}

void do_accept(SOCKET s_socket) {
	EXP_OVER* accept_over = new EXP_OVER(IO_ACCEPT);
	SOCKET c_socket = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, 0, 0, WSA_FLAG_OVERLAPPED);

	// ���� �ɼ� ���� (Nagle �˰��� ��Ȱ��ȭ)
	int opt = 1;
	setsockopt(c_socket, IPPROTO_TCP, TCP_NODELAY, (char*)&opt, sizeof(opt));

	accept_over->_accept_socket = c_socket;

	// AcceptEx ȣ��
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


// Worker Thread �ڵ鷯
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

			// 1. Ŭ���̾�Ʈ �ּ� ���� ����
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

			// 2. IP �ּ� ���ڿ� ��ȯ
			char ip_str[INET_ADDRSTRLEN];
			inet_ntop(AF_INET, &(client_addr->sin_addr), ip_str, INET_ADDRSTRLEN);
			std::cout << "[����] ���ο� Ŭ���̾�Ʈ ����: IP=" << ip_str
				<< ", ��Ʈ=" << ntohs(client_addr->sin_port)
				<< ", �Ҵ� ID=" << new_id << "\n";

			// 3. IOCP�� ���� ���
			CreateIoCompletionPort(reinterpret_cast<HANDLE>(client_socket), g_hIOCP, new_id, 0);

			// 4. ���� ����
			new SESSION(new_id, client_socket);

			// 5. ���� Accept ��û
			do_accept(g_listen_socket);

			// 6. ���� OVERLAPPED �޸� ����
			delete eo;
			break;

		}
		case IO_SEND:
			delete eo;
			break;
		case IO_RECV:
			// 1. ���ؽ� ������ ���� �˻� (������ ������)
			SESSION* pUser = nullptr;
			{
				std::lock_guard<std::mutex> lock(g_session_mutex);
				auto it = g_sessions.find(key);
				if (it == g_sessions.end()) {
					// ������ �̹� ���ŵ� ���
					delete eo;  // EXP_OVER ��ü ����
					continue;
				}
				pUser = it->second;  // ������ ����
			}

			if (FALSE == ret || 0 == io_size) {
				std::cout << "[����] " << key << "�� Ŭ���̾�Ʈ ���� ����\n";

				std::lock_guard<std::mutex> lock(g_session_mutex);
				if (g_sessions.count(key)) {
					SESSION* session = g_sessions[key];
					closesocket(session->_c_socket);  // ���� �ݱ�
					delete session;                   // ���� ��ü �޸� ����
					g_sessions.erase(key);            // �ʿ��� ����
				}
				delete eo;  // EXP_OVER ��ü ����
				continue;
			}

			// 2. ���� �۾� (���� ������ ���¿��� ����)
			SESSION& user = *pUser;  // ������

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

	