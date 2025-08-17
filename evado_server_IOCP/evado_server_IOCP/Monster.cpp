#include "Monster.h"

Spider::Spider(int64_t id, XMFLOAT3 pos, uint8_t state, int hp)
    : _monsterID(id), _position(pos), _state(state), _hp(hp) {}



// A* 알고리즘 적용
bool Spider::Update(float dt, const XMFLOAT3& playerPos, const bool map[MAP_HEIGHT][MAP_WIDTH]) {
    float dx = playerPos.x - _position.x;
    float dz = playerPos.z - _position.z;
    float distance = sqrtf(dx * dx + dz * dz);

    const float aggroRange = 8.0f;
    const float attackRange = 3.0f;
    const float moveSpeed = 3.5f;

    _attackCooldown -= dt;
    bool pathChanged = false;

    if (distance <= aggroRange) {
        if (distance > attackRange) {
            // 1. 목표 타일 좌표 계산
            TileCoord playerTile = WorldToTile(playerPos.x, playerPos.z);

            // x/y가 벗어나면 패스
            if (playerTile.x < 0 || playerTile.x >= MAP_WIDTH ||
                playerTile.y < 0 || playerTile.y >= MAP_HEIGHT)
                return false;

            // 2. 경로 없거나, 목적지가 바뀌면 재탐색
            if (!_hasLastTarget || !(playerTile == _lastTargetTile)) {
                FindPath(playerTile, map);
                _lastTargetTile = playerTile;
                _hasLastTarget = true;
                pathChanged = true;
            }

            // 3. 경로따라 이동
            if (!_path.empty()) {
                TileCoord next = _path.front();
                if (!map[next.y][next.x]) {
                    _path = {};
                    _hasLastTarget = false;
                    _state = static_cast<uint8_t>(MonsterAnimationState::IDLE);
                    return false;
                }
                float nextX, nextZ;
                TileToWorld(next.x, next.y, nextX, nextZ);

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

                    float lookDx = playerPos.x - _position.x;
                    float lookDz = playerPos.z - _position.z;
                    float lookYaw = atan2f(lookDx, lookDz);
                    SetRotation({ 0.f, lookYaw, 0.f });

                    _state = static_cast<uint8_t>(MonsterAnimationState::WALK);
                }
            }
            if (distance <= attackRange) {
                float lookDx = playerPos.x - _position.x;
                float lookDz = playerPos.z - _position.z;
                float lookYaw = atan2f(lookDx, lookDz);
                SetRotation({ 0.f, lookYaw, 0.f });

                _state = static_cast<uint8_t>(MonsterAnimationState::ATTACK);
                if (_attackCooldown <= 0.0f) {
                    _attackCooldown = 1.0f;
                    return true; // 공격 성공!
                }
            }

            return false;
        }
        else {
            _state = static_cast<uint8_t>(MonsterAnimationState::WALK);

            _patrolCooldown -= dt;

            if (!_hasPatrolTarget || _patrolCooldown <= 0.0f) {
                // 랜덤 좌표 선택, 맵 안의 걸을 수 있는 타일만 선택
                int attempts = 0;
                do {
                    int x = rand() % MAP_WIDTH;
                    int y = rand() % MAP_HEIGHT;
                    if (map[y][x]) {
                        _patrolTarget = TileCoord{ x, y };
                        _hasPatrolTarget = true;
                        break;
                    }
                    attempts++;
                } while (attempts < 20); // 맵이 좁은경우 무한루프 방지
                _patrolCooldown = 1.0f + static_cast<float>(rand()) / RAND_MAX * 1.5f; // 1~2.5초마다 목표 갱신
                FindPath(_patrolTarget, map);
            }
            // 목적지 이동
            if (!_path.empty()) {
                TileCoord next = _path.front();
                if (!map[next.y][next.x]) {
                    _path = {};
                    _hasPatrolTarget = false;
                    _state = static_cast<uint8_t>(MonsterAnimationState::IDLE);
                    return false;
                }
                float nextX, nextZ;
                TileToWorld(next.x, next.y, nextX, nextZ);

                float ddx = nextX - _position.x;
                float ddz = nextZ - _position.z;
                float len = sqrtf(ddx * ddx + ddz * ddz);
                if (len < 0.05f) {
                    _path.pop(); // 도착
                    if (_path.empty()) _hasPatrolTarget = false;
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
        }
        return false;
     }
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
        for (const auto& tile : rev_path) {
            q.push(tile);
        }
    }
    _path = q;
}