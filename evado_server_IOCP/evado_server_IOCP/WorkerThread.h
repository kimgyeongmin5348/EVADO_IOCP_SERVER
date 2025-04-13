#pragma once

#include "Common.h"


constexpr short SERVER_PORT = 3000;

enum IO_OP { IO_RECV, IO_SEND, IO_ACCEPT };

class SESSION;

extern HANDLE g_hIOCP;
extern std::unordered_map<long long, SESSION> g_users;

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

    short			_x, _y, _z;
    std::string		_name;

public:
    SESSION();
    SESSION(long long session_id, SOCKET s);
    ~SESSION();

    void do_recv();
    void do_send(void* buff);
    void send_player_info_packet();
    void send_player_position();
    void process_packet(unsigned char* p);
};

void print_error_message(int s_err);
void do_accept(SOCKET s_socket, EXP_OVER* accept_over);