#include "Common.h"
#include "Monster.h"
#include "workerthread.h"
#include "DBConnect.h"
#include "DBConnectPool.h"

//------------------- Check List --------------------- 
// 
//	1. 플레이어 정보 송수신 [ 2025.07.22 ]
//		- 위치, 시선, 애니메이션, hp, cash 정보 (완)
//		- 플레이어가 가지고 있는 돈 <- 일단 해야 할것들 해두고 주석 처리 함. 집가서 db테이블 수정하고 주석 풀기.
// 
// 
// 
//	2. NPC(몬스터) ai 만들기 
//		- 평소엔 가만히 있게 하는게 아니라 움직이게 해보자. (지금은 가만히 있는다. 플레이어가 주변에 와야 움직임) < 2025.07.22 >
//		- A* 구현중에 있음 (95%) [랜더링 되면 정확한 확인이 필요함] < 2025.07.22 >
//		- 아이템(삽) 이랑 충돌이 일어나면 클라에서 그 정보를 서버에 보내고 서버는 받은 정보로 몬스터의 hp를 깍고 그 정보를 다른 클라이언트에게로 보낸다. < 2025.07.26 >
// 
// 
//	3. 아이템 스폰 설정 
//		- 아이템의 값어치 설정함. < 2025.07.22 >
// 
// 
// 
//	4. 플레이어 정보 저장 
//		-DB에 저장 할려고 했지만 시간상? 부족하여 지금은 컨테이너에 저장하게 하자.. < 2025.07.22 >
// 
// 
// 
//	5. 상점
//		- 상점 관련 패킷과 함수들 생성함. < 2025.07.22 >
//		- 인벤토리나 상점 UI 생성되면 메커니즘을 수정해보거나 생각을 한번 더 해봐야 할것 같음. < 2025.07.22 >
// 
//----------------------------------------------------



int main()
{

	/*if (!dbPool.Connect(20)) {
		cout << "DB Connection Failed!" << endl;
		return -1;
	}*/

	InitializeWorldMap();

	std::wcout.imbue(std::locale("korean"));

	WSADATA WSAData;
	if (WSAStartup(MAKEWORD(2, 0), &WSAData) != 0) {
		std::cerr << "[ERROR] WSAStartup 실패" << std::endl;
		return 1;
	}


	// 1. 리스닝 소켓 생성
	g_listen_socket = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, 0, 0, WSA_FLAG_OVERLAPPED);
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
	do_accept(g_listen_socket);
	TestSpawnMultipleItems(); // 아이템 생성
	InitializeMonsters(); // 몬스터 생성

	// 몬스터 관련 스레드
	std::thread game_loop_thread(MGameLoopThread);

	// 4. 워커 스레드 생성 및 메인 스레드 대기
	std::cout << "서버 시작" << std::endl;
	auto num_threads = (std::min)(8u, std::thread::hardware_concurrency());
	std::vector<std::thread> workers;

	for (unsigned int i = 0; i < num_threads; ++i)
		workers.emplace_back(WorkerThread);
	for (auto& w : workers)
		w.join();

	// 게임루프 쓰레드
	game_loop_thread.join();

	closesocket(g_listen_socket);
	WSACleanup();
}