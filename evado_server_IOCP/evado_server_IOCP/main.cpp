#include "Common.h"
#include "workerthread.h"



int main()
{
	std::wcout.imbue(std::locale("korean"));

	WSADATA WSAData;
	if (WSAStartup(MAKEWORD(2, 0), &WSAData) != 0) {
		std::cerr << "[ERROR] WSAStartup 실패" << std::endl;
		return 1;
	}


	// 1. 리스닝 소켓 생성
	SOCKET g_listen_socket = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, 0, 0, WSA_FLAG_OVERLAPPED);;
	if (g_listen_socket <= 0) std::cout << "ERROR" << "원인";
	else std::cout << "Socket Created.\n";

	SOCKADDR_IN addr;
	addr.sin_family = AF_INET;
	addr.sin_port = htons(SERVER_PORT);
	addr.sin_addr.s_addr = htonl(INADDR_ANY);
	bind(g_listen_socket, reinterpret_cast<sockaddr*>(&addr), sizeof(SOCKADDR_IN));
	listen(g_listen_socket, SOMAXCONN);
	INT addr_size = sizeof(SOCKADDR_IN);

	// 2. IOCP 생성 및 리스닝 소켓 연결
	g_hIOCP = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, NULL, 0);
	CreateIoCompletionPort(reinterpret_cast<HANDLE>(g_listen_socket), g_hIOCP, -1, 0);


	// 3. 초기 Accept 시작
	do_accept(g_listen_socket, &g_accept_over);

	// 4. 워커 스레드 생성 및 메인 스레드 대기
	std::cout << "서버 시작" << std::endl;
	auto num_threads = (std::min)(8u, std::thread::hardware_concurrency());
	std::vector<std::thread> workers;
	for (unsigned int i = 0; i < num_threads; ++i)
		workers.emplace_back(WorkerThread);
	for (auto& w : workers)
		w.join();

	closesocket(g_listen_socket);
	WSACleanup();
}