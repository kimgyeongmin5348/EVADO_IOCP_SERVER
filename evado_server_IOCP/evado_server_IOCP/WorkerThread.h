#pragma once

#include "Common.h"

class SESSION;

// ���� ���� ����
extern HANDLE g_hIOCP;
extern std::atomic<long long> g_client_counter;
extern std::unordered_map<long long, SESSION*> g_sessions;  //������ �ڵ忡�� g_user�� �Ǿ� ����.
extern std::mutex g_session_mutex;
extern SOCKET g_listen_socket;


enum IO_OP { IO_RECV, IO_SEND, IO_ACCEPT };

//class SESSION;

class EXP_OVER
{
public:
	EXP_OVER(IO_OP op);


	WSAOVERLAPPED	_over;
	IO_OP			_io_op;
	SOCKET			_accept_socket;
	unsigned char	_buffer[1024];
	WSABUF			_wsabuf[1];


};

extern EXP_OVER g_accept_over;


class SESSION {
public:
	SOCKET			_c_socket;
	long long		_id;

	EXP_OVER		_recv_over{ IO_RECV };
	unsigned char	_remained;

	XMFLOAT3		_position;
	//XMFLOAT3		_look;
	//XMFLOAT3		_right;
	std::string		_name;

public:
	SESSION();
	SESSION(long long session_id, SOCKET s);
	~SESSION();

	void do_recv();
	void do_send(void* buff);
	void send_player_info_packet();
	void broadcast_move_packet();
	void process_packet(unsigned char* p);
	
};

void print_error_message(int s_err);
void do_accept(SOCKET s_socket, EXP_OVER* accept_over);
void WorkerThread();