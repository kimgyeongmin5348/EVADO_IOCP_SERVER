#include "workerthread.h"
#include "Common.h" // ���� ��� ���� ����


// ���� ���� �ʱ�ȭ
HANDLE g_hIOCP;
std::atomic<long long> g_client_counter = 0;
std::unordered_map<long long, SESSION*> g_sessions;
std::mutex g_session_mutex;
SOCKET g_listen_socket = INVALID_SOCKET;
EXP_OVER g_accept_over{ IO_ACCEPT };

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
	std::lock_guard<std::mutex> lock(g_session_mutex);

	// �ߺ� ID üũ
	if (g_sessions.find(_id) != g_sessions.end()) {
		std::cerr << "�ߺ� ���� ID: " << _id << std::endl;
		closesocket(_c_socket);
		delete this; // �޸� ���� ����
		return;
	}

	g_sessions[_id] = this; //������ ����
	_remained = 0;
	do_recv();
}

// leave ���� ����
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
	sc_packet_user_info p;
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
	const unsigned char packet_type = p[1];
	switch (packet_type) {
	case CS_P_LOGIN: 
	{
		cs_packet_login* packet = reinterpret_cast<cs_packet_login*>(p);
		_name = packet->name;
		_position = { 0.0,0.0,0.0 };
		send_player_info_packet();

		std::cout << "[����] " << _id << "�� Ŭ�� �α���: " << _name << std::endl;

		sc_packet_enter ep;
		ep.size = sizeof(ep);
		ep.type = SC_P_ENTER;
		ep.id = _id;
		strcpy_s(ep.name, _name.c_str());
		ep.o_type = 0;
		ep.position = _position;

		for (auto& u : g_sessions) {
			if (u.first != _id)
				u.second->do_send(&ep);
		}

		for (auto& u : g_sessions) {
			if (u.first != _id) {
				sc_packet_enter other_ep; // ���� �̸� ���
				other_ep.size = sizeof(other_ep);
				other_ep.type = SC_P_ENTER;
				other_ep.id = u.first;
				strcpy_s(other_ep.name, u.second->_name.c_str());
				other_ep.o_type = 0;
				other_ep.position = u.second->_position;
				do_send(&other_ep);
			}
		}
		break;
	}

	case CS_P_MOVE: { // �̺κ��� Ŭ���̾�Ʈ�� �̾߱� �ϸ鼭 ���ĺ��� ����. ( �� �������� ��� �ؼ� �����°�, Ŭ������ �׳� ��ǥ�� ���ڶ�� ����)
		cs_packet_move* packet = reinterpret_cast<cs_packet_move*>(p);


		sc_packet_move mp;
		mp.size = sizeof(mp);
		mp.type = SC_P_MOVE;
		mp.id = _id;
		mp.position = _position;
		for (auto& u : g_sessions) {
			if (u.first != _id)
				u.second->do_send(&mp);
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

void do_accept(SOCKET s_socket, EXP_OVER* accept_over)
{
	SOCKET c_socket = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, 0, 0, WSA_FLAG_OVERLAPPED);;
	if (INVALID_SOCKET == c_socket) {
		std::cerr << "���� ���� ����: " << WSAGetLastError() << std::endl;
		return;
	}
	accept_over->_accept_socket = c_socket;
	AcceptEx(s_socket, c_socket, accept_over->_buffer, 0,
		sizeof(SOCKADDR_IN) + 16, sizeof(SOCKADDR_IN) + 16,
		NULL, &accept_over->_over);
}


//EXP_OVER g_accept_over{ IO_ACCEPT };

// Worker Thread �ڵ鷯
void WorkerThread() {
	while (true) {
		DWORD io_size;
		WSAOVERLAPPED* o;
		ULONG_PTR key;
		BOOL ret = GetQueuedCompletionStatus(g_hIOCP, &io_size, &key, &o, INFINITE);
		EXP_OVER* eo = reinterpret_cast<EXP_OVER*>(o);

		if (FALSE == ret || ((eo->_io_op == IO_RECV || eo->_io_op == IO_SEND) && (0 == io_size))) {
			std::lock_guard<std::mutex> lock(g_session_mutex);
			auto it = g_sessions.find(key);
			if (it != g_sessions.end()) {
				delete it->second;  // �޸� ����
				g_sessions.erase(it);
			}
			continue;
		}

		switch (eo->_io_op)
		{
		case IO_ACCEPT:
		{
			int new_id = g_new_id++;
			SOCKET client_socket = eo->_accept_socket;
			// �� ���� ���� �Ҵ�
			SESSION* new_session = new SESSION(new_id, client_socket);
			// IOCP�� ���� ���
			CreateIoCompletionPort(reinterpret_cast<HANDLE>(client_socket), g_hIOCP, new_id, 0);
			do_accept(g_listen_socket, &g_accept_over);
		}
		break;
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
					return;
				}
				pUser = it->second;  // ������ ����
			}  // ���⼭ �� �ڵ� ����

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

	