#pragma once

#include "Common.h"

class SESSION;

// 전역 변수 선언
extern HANDLE g_hIOCP;
extern std::atomic<long long> g_client_counter;
extern std::unordered_map<long long, SESSION*> g_sessions;  //교수님 코드에선 g_user로 되어 있음.
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
	std::atomic<bool> _is_sending{ false };

public:
	SESSION();
	SESSION(long long session_id, SOCKET s);
	~SESSION();

	void do_recv();
	void do_send(void* buff);
	void send_player_info_packet();	
	void process_packet(unsigned char* p);
	void HandleItemPickup(long long item_id);
	
};

void BroadcastToAll(void* pkt, long long exclude_id = -1);
void safe_remove_session(long long id);
void print_error_message(int s_err);
void do_accept(SOCKET s_socket);
void WorkerThread();

void TestSpawnMultipleItems();