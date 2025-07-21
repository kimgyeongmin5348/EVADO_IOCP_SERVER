#include "Monster.h"

Spider::Spider(int64_t id, XMFLOAT3 pos, uint8_t state)
    : _monsterID(id), _position(pos), _state(state) {}


//bool Spider::Update(float fTimeElapsed, const XMFLOAT3& playerPos) {
//
//    float dx = playerPos.x - _position.x;
//    float dz = playerPos.z - _position.z;
//    float distance = sqrtf(dx * dx + dz * dz);
//
//    // 인식 범위
//    const float aggroRange = 3.0f;
//    // 공격 범위
//    const float attackRange = 2.0f;
//    // 이동 속도  <- 플레이어 이동속도 보고 설정해보자.
//    const float moveSpeed = 2.5f;
//
//    _attackCooldown -= fTimeElapsed;
//
//    if (distance <= aggroRange) {
//        if (distance > attackRange) {
//            // 추적(이동)
//            _state = static_cast<uint8_t>(MonsterAnimationState::WALK);
//
//            float dirX = dx / distance;
//            float dirZ = dz / distance;
//
//            _position.x += dirX * moveSpeed * fTimeElapsed;
//            _position.z += dirZ * moveSpeed * fTimeElapsed;
//            return false;
//        }
//        else {
//            // 공격 로직
//            _state = static_cast<uint8_t>(MonsterAnimationState::ATTACK);
//
//            if (_attackCooldown <= 0.0f) {
//                _attackCooldown = 1.0f; // 1초 쿨타임
//                return true; // 공격 성공!
//            }
//            std::cout << "[몬스터] : 공격 ! " << '\n';
//
//            
//        }
//    }
//    else {
//        _state = static_cast<uint8_t>(MonsterAnimationState::IDLE);
//    }
//}


// A* 알고리즘 적용
bool Spider::Update(float dt, const XMFLOAT3& playerPos, const bool map[MAP_HEIGHT][MAP_WIDTH]) {
    float dx = playerPos.x - _position.x;
    float dz = playerPos.z - _position.z;
    float distance = sqrtf(dx * dx + dz * dz);

    const float aggroRange = 40.0f;
    const float attackRange = 2.0f;
    const float moveSpeed = 2.5f;

    _attackCooldown -= dt;

    if (distance <= aggroRange) {
        if (distance > attackRange) {
            // 1. 목표 타일 좌표 계산
            TileCoord playerTile = WorldToTile(playerPos.x, playerPos.z);

            // x/y가 벗어나면 패스 (디펜시브)
            if (playerTile.x < 0 || playerTile.x >= MAP_WIDTH ||
                playerTile.y < 0 || playerTile.y >= MAP_HEIGHT)
                return false;

            // 2. 경로 없거나, 목적지가 바뀌면 재탐색
            if (_path.empty() || _path.back() != playerTile) {
                FindPath(playerTile, map);
            }

            // 3. 경로따라 이동
            if (!_path.empty()) {
                TileCoord next = _path.front();
                if (!map[next.y][next.x]) {
                    std::cout << "[몬스터] 장애물 충돌 발생: (" << next.x << ", " << next.y << ")" << std::endl;
                    _path = {}; // 경로 초기화(혹은 idle 전환 등 처리)
                    _state = static_cast<uint8_t>(MonsterAnimationState::IDLE);
                    return false;
                }
                float nextX, nextZ;
                TileToWorld(next.x, next.y, nextX, nextZ);

                std::cout << "[몬스터] " << _monsterID << " 현재 위치("
                    << _position.x << ", " << _position.z
                    << ") -> 다음 타일(" << next.x << "," << next.y
                    << ") | 목표좌표(" << nextX << "," << nextZ << ")\n";

                float ddx = nextX - _position.x;
                float ddz = nextZ - _position.z;
                float len = sqrtf(ddx * ddx + ddz * ddz);
                if (len < 0.05f) {
                    _path.pop(); // 타일 도착
                }
                else {
                    ddx /= len; ddz /= len;
                    _position.x += ddx * moveSpeed * dt;
                    _position.z += ddz * moveSpeed * dt;
                    _state = static_cast<uint8_t>(MonsterAnimationState::WALK);
                }
            }
            return false;
        }
        else {
            // 공격
            _state = static_cast<uint8_t>(MonsterAnimationState::ATTACK);
            if (_attackCooldown <= 0.0f) {
                _attackCooldown = 1.0f;
                return true; // 공격 성공!
            }
            std::cout << "[몬스터] : 공격 ! " << '\n';
        }
    }
    else {
        _state = static_cast<uint8_t>(MonsterAnimationState::IDLE);
    }
    return false;
}

// A* 알고리즘
void Spider::FindPath(const TileCoord& to, const bool map[MAP_HEIGHT][MAP_WIDTH]) {

    // 현재 몬스터 위치 -> 시작 타일 좌표
    TileCoord start = WorldToTile(_position.x, _position.z);

    // (옵셔널) 경계 확인
    if (start.x < 0 || start.x >= MAP_WIDTH ||
        start.y < 0 || start.y >= MAP_HEIGHT)
    {
        _path = {};
        std::cout << "[A*] 시작 위치가 맵 범위를 벗어남!" << std::endl;
        return;
    }
    if (to.x < 0 || to.x >= MAP_WIDTH ||
        to.y < 0 || to.y >= MAP_HEIGHT)
    {
        _path = {};
        std::cout << "[A*] 목표 위치가 맵 범위를 벗어남!" << std::endl;
        return;
    }


    std::cout << "[A*] 몬스터 ID: " << _monsterID
        << " 경로탐색 시작 (" << start.x << ", " << start.y << ") -> ("
        << to.x << ", " << to.y << ")" << std::endl;

    // A* 노드 내부 구조체
    struct Node {
        int x, y;
        float g, h;
        Node* parent;
        Node(int x_, int y_, float g_, float h_, Node* p_) : x(x_), y(y_), g(g_), h(h_), parent(p_) {}
    };

    auto heuristic = [](int ax, int ay, int bx, int by) {
        // 맨해튼 거리(4방향)
        return std::abs(ax - bx) + std::abs(ay - by);
        };

    // 우선순위큐(최솟값 기준)
    auto cmp = [](const Node* a, const Node* b) { return (a->g + a->h) > (b->g + b->h); };
    std::priority_queue<Node*, std::vector<Node*>, decltype(cmp)> open(cmp);

    // 방문 체크 및 부모 노드 정보
    bool closed[MAP_HEIGHT][MAP_WIDTH] = { false };

    // 시작 노드 정보
    Node* startNode = new Node(start.x, start.y, 0, heuristic(start.x, start.y, to.x, to.y), nullptr);
    open.push(startNode);

    Node* goalNode = nullptr;

    // A* 주요 루프
    while (!open.empty()) {
        Node* current = open.top(); open.pop();

        // 목적지 도달
        if (current->x == to.x && current->y == to.y) {
            goalNode = current;
            break;
        }
        closed[current->y][current->x] = true;

        // 네 방향 체크 (상,하,좌,우)
        const int dx[4] = { 1, -1, 0, 0 };
        const int dy[4] = { 0, 0, 1, -1 };
        for (int dir = 0; dir < 4; ++dir) {
            int nx = current->x + dx[dir];
            int ny = current->y + dy[dir];

            // 영역 벗어남 장애물 이미 방문했다면 continue
            if (nx < 0 || ny < 0 || nx >= MAP_WIDTH || ny >= MAP_HEIGHT) continue;
            if (!map[ny][nx] || closed[ny][nx]) continue;

            float ng = current->g + 1;
            float nh = heuristic(nx, ny, to.x, to.y);
            open.push(new Node(nx, ny, ng, nh, current));
        }
    }

    // 경로 못 찾은 경우
    if (!goalNode) {
        std::cout << "[A*] 경로 못 찾음!" << std::endl;
        while (!open.empty()) { delete open.top(); open.pop(); }
        _path = {}; // 경로 없음
        return;
    }

    // 경로 역추적 - 리스트로 담은 뒤 큐로 변환
    std::vector<TileCoord> rev_path;
    Node* node = goalNode;
    while (node) {
        rev_path.push_back({ node->x, node->y });
        node = node->parent;
    }

    // open/closed에 남은 노드 해제
    // (생략 가능; 실제 서비스에선 smart pointer 권장)
    // 여기서는 new한 노드가 open엔 남아 있을 수 있으니 반드시 모두 해제 필요

    // 경로 뒤집어 큐에 push(시작점->도착점 순서)
    std::reverse(rev_path.begin(), rev_path.end());

    std::cout << "[A*] 경로 길이: " << rev_path.size();
    if (!rev_path.empty())
        std::cout << " (도착: " << rev_path.back().x << ", " << rev_path.back().y << ")";
    std::cout << "\n[A*] 경로:";
    for (auto& t : rev_path)
        std::cout << " -> (" << t.x << "," << t.y << ")";
    std::cout << std::endl;

    std::queue<TileCoord> q;
    // 첫 번째 타일은 현재 타일=생략하고, 다음부터 q에 push
    for (size_t i = 1; i < rev_path.size(); ++i) {
        q.push(rev_path[i]);
    }
    _path = q;

}