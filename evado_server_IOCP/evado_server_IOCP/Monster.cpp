#include "Monster.h"

Spider::Spider(int64_t id, XMFLOAT3 pos, uint8_t state, int hp)
    : _monsterID(id), _position(pos), _state(state), _hp(hp) {}


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

    const float aggroRange = 15.0f;
    const float attackRange = 2.0f;
    const float moveSpeed = 4.5f;

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
                FindPath(playerTile, map);  // A* 알고리즘 적용
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

             /*   std::cout << "[몬스터] " << _monsterID << " 현재 위치("
                    << _position.x << ", " << _position.z
                    << ") -> 다음 타일(" << next.x << "," << next.y
                    << ") | 목표좌표(" << nextX << "," << nextZ << ")\n";*/

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

                    float yaw = atan2f(ddx, ddz);
                    SetRotation({ 0.f, yaw, 0.f });

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
           
        }
    }
    else {
        _state = static_cast<uint8_t>(MonsterAnimationState::IDLE);
    }
    return false;
}

// A* 알고리즘
void Spider::FindPath(const TileCoord& to, const bool map[MAP_HEIGHT][MAP_WIDTH]) {
    TileCoord start = WorldToTile(_position.x, _position.z);

    if (start.x < 0 || start.x >= MAP_WIDTH || start.y < 0 || start.y >= MAP_HEIGHT) return;
    if (to.x < 0 || to.x >= MAP_WIDTH || to.y < 0 || to.y >= MAP_HEIGHT) return;

    // 노드 정보 2차원 배열
    NodeInfo nodes[MAP_HEIGHT][MAP_WIDTH];
    auto heuristic = [](int ax, int ay, int bx, int by) {
        return float(abs(ax - bx) + abs(ay - by));
        };

    std::priority_queue<OpenNode> open;
    nodes[start.y][start.x].g = 0;
    open.push({ start.x, start.y, heuristic(start.x, start.y, to.x, to.y) });

    constexpr int dx[4] = { 1, -1, 0, 0 };
    constexpr int dy[4] = { 0, 0, 1, -1 };
    bool found = false;

    while (!open.empty()) {
        auto [cx, cy, f] = open.top(); open.pop();
        if (nodes[cy][cx].closed) continue; // 이미 최적 경로로 방문 완료

        nodes[cy][cx].closed = true;
        if (cx == to.x && cy == to.y) { found = true; break; }

        for (int dir = 0; dir < 4; ++dir) {
            int nx = cx + dx[dir], ny = cy + dy[dir];
            if (nx < 0 || ny < 0 || nx >= MAP_WIDTH || ny >= MAP_HEIGHT) continue;
            if (!map[ny][nx]) continue;            // 장애물
            if (nodes[ny][nx].closed) continue;    // 이미 방문

            float ng = nodes[cy][cx].g + 1;
            if (ng < nodes[ny][nx].g) { // 더 짧은 경로면 갱신
                nodes[ny][nx].g = ng;
                nodes[ny][nx].parentX = cx;
                nodes[ny][nx].parentY = cy;
                float h = heuristic(nx, ny, to.x, to.y);
                open.push({ nx, ny, ng + h });
            }
        }
    }

    // 경로 역추적
    std::queue<TileCoord> q;
    if (found) {
        int x = to.x, y = to.y;
        std::vector<TileCoord> rev_path;
        while (!(x == start.x && y == start.y)) {
            rev_path.push_back({ x,y });
            int px = nodes[y][x].parentX, py = nodes[y][x].parentY;
            if (px == -1 || py == -1) break; // 예외 방지
            x = px; y = py;
        }
        std::reverse(rev_path.begin(), rev_path.end());

      /*  std::cout << "[A* 경로] 몬스터ID: " << _monsterID << " 경로: ";
        for (const auto& tile : rev_path) {
            std::cout << " -> (" << tile.x << "," << tile.y << ")";
        }
        std::cout << std::endl;*/

        for (const auto& tile : rev_path) {
            q.push(tile);
        }
    }
    _path = q;
}