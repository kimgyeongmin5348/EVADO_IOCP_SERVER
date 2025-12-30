# EVADO_IOCP_SERVER

멀티플레이어 게임 서버는 수백 명의 동시 접속 플레이어를 실시간으로 처리하도록 설계되었으며, Windows I/O Completion Ports(IOCP)를 사용한 네트워킹, AI 경로찾기, 데이터베이스 통합, 그리고 아이템 관리, 몬스터 AI, 플레이어 상호작용을 포함한 포괄적인 게임 메커니즘을 특징으로 합니다.


<img width="841" height="532" alt="image" src="https://github.com/user-attachments/assets/42c9ab8e-88c8-428c-9e91-480cfa0d8a3a" />


세션 관리 시스템

세션 관리 시스템은 원자적 연산과 뮤텍스 동기화를 사용하여 클라이언트 연결의 스레드 안전 처리를 제공합니다.

ex)

std::atomic<long long> g_client_counter = 0;

std::unordered_map<long long, SESSION*> g_sessions;

std::mutex g_session_mutex;




AI 경로찾기

A 알고리즘 구현:*

휴리스틱 최적화: 효율적인 경로찾기를 위한 맨해튼 거리 계산

장애물 회피: 맵 데이터로부터의 동적 장애물 감지

메모리 효율성: 재사용 가능한 경로찾기 데이터 구조

성능 최적화: 캐시된 경로 계산 및 증분 업데이트




메모리 관리

서버는 최적의 성능을 보장하기 위해 여러 메모리 관리 전략을 사용




스레드 분배:

워커 스레드 (40%): IOCP I/O 작업 처리

게임 루프 스레드 (25%): 핵심 게임 로직 및 상태 업데이트 관리

몬스터 AI 스레드 (15%): AI 계산 및 경로찾기 처리

데이터베이스 스레드 (10%): 모든 데이터베이스 작업 처리

네트워크 Accept 스레드 (5%): 새 클라이언트 연결 관리

시스템 스레드 (5%): 로깅, 모니터링, 유지보수 작업
