#include "workerthread.h"
#include "Common.h" // 공통 헤더 파일 포함

std::atomic<long long> g_client_counter = 0;

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

SESSION::SESSION(long long session_id, SOCKET s) : _id(session_id), _c_socket(s), _recv_over(IO_RECV)
{
	_remained = 0;
	do_recv();
}

SESSION::~SESSION()
{
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
	memcpy(o->_buffer, buff, packet_size);
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
	strncpy(p.name, _name.c_str(), MAX_ID_LENGTH - 1);
	p.name[MAX_ID_LENGTH - 1] = '\0';
	p.position = _position;
	//p.look = _look;
	//p.right = _right;
	//p.hp = 100;
	do_send(&p);
}

void SESSION::send_player_position()
{
	sc_packet_move p;
	p.id = _id;
	p.size = sizeof(p);
	p.type = SC_P_MOVE;
	strncpy(p.name, _name.c_str(), MAX_ID_LENGTH - 1);
	p.name[MAX_ID_LENGTH - 1] = '\0';
	do_send(&p);
}

void SESSION::process_packet(unsigned char* p)
{
	const unsigned char packet_type = p[1];
	switch (packet_type) {
	case CS_P_LOGIN:
	{
		cs_packet_login* packet = reinterpret_cast<cs_packet_login*>(p);
		_name.assign(packet->name, MAX_ID_LENGTH);

		send_player_info_packet();
		break;
	}
	case CS_P_MOVE: {
		cs_packet_move* packet = reinterpret_cast<cs_packet_move*>(p);

		// ▶ 좌표 직접 적용
		_position = packet->position;
		

		send_player_position(); // 다른 클라이언트에 전송
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
	SOCKET c_socket = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, 0, 0, WSA_FLAG_OVERLAPPED);
	accept_over->_accept_socket = c_socket;
	CreateIoCompletionPort(reinterpret_cast<HANDLE>(c_socket), g_hIOCP, 3, 0);
	AcceptEx(s_socket, c_socket, accept_over->_buffer, 0,
		sizeof(SOCKADDR_IN) + 16, sizeof(SOCKADDR_IN) + 16,
		NULL, &accept_over->_over);
}