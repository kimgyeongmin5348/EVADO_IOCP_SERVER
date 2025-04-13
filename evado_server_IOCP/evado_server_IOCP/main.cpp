#include "Common.h"
#include "workerthread.h"


HANDLE g_hIOCP;
std::unordered_map<long long, SESSION> g_users;

int main()
{
    std::wcout.imbue(std::locale("korean"));

    WSADATA WSAData;
    WSAStartup(MAKEWORD(2, 0), &WSAData);

    SOCKET s_socket = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, 0, 0, WSA_FLAG_OVERLAPPED);
    if (s_socket <= 0) std::cout << "ERROR" << "¿øÀÎ";
    else std::cout << "Socket Created.\n";

    SOCKADDR_IN addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(SERVER_PORT);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    bind(s_socket, reinterpret_cast<sockaddr*>(&addr), sizeof(SOCKADDR_IN));
    listen(s_socket, SOMAXCONN);
    INT addr_size = sizeof(SOCKADDR_IN);

    g_hIOCP = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, NULL, 0);
    CreateIoCompletionPort(reinterpret_cast<HANDLE>(s_socket), g_hIOCP, -1, 0);

    EXP_OVER accept_over(IO_ACCEPT);

    do_accept(s_socket, &accept_over);

    int new_id = 0;
    while (true) {
        DWORD io_size;
        WSAOVERLAPPED* o;
        ULONG_PTR key;
        BOOL ret = GetQueuedCompletionStatus(g_hIOCP, &io_size, &key, &o, INFINITE);
        EXP_OVER* eo = reinterpret_cast<EXP_OVER*>(o);
        switch (eo->_io_op) {
        case IO_ACCEPT:
            g_users.try_emplace(new_id, new_id, eo->_accept_socket);
            new_id++;
            do_accept(s_socket, &accept_over);
            break;
        case IO_SEND:
            delete eo;
            break;
        case IO_RECV:
            SESSION& user = g_users[key];

            unsigned char* p = eo->_buffer;
            int data_size = io_size + user._remained;

            while (p < eo->_buffer + data_size) {
                unsigned char packet_size = *p;
                if (p + packet_size > eo->_buffer + data_size)
                    break;
                user.process_packet(p);
                p = p + packet_size;
            }

            if (p < eo->_buffer + data_size)
            {
                user._remained = static_cast<unsigned char>(eo->_buffer + data_size - p);
                memcpy(p, eo->_buffer, user._remained);
            }
            else
                user._remained = 0;

            user.do_recv();
            break;
        }
    }
    closesocket(s_socket);
    WSACleanup();
}