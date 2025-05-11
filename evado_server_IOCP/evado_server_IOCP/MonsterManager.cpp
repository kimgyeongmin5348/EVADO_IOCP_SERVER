#include "MonsterManager.h"
#include "WorkerThread.h"
#include "Common.h"

MonsterManager& MonsterManager::GetInstance() {
    static MonsterManager instance;
    return instance;
}

MonsterManager::~MonsterManager() {
    std::lock_guard<std::mutex> lock(_mutex);
    for (auto& pair : _monsters) {
        delete pair.second;
    }
    _monsters.clear();
}

void MonsterManager::SpawnMonster(int64_t id, const XMFLOAT3& pos, uint8_t state) {
    std::lock_guard<std::mutex> lock(_mutex);

    if (_monsters.find(id) != _monsters.end()) {
        std::cerr << "Monster ID " << id << " already exists!\n";
        return;
    }

    Spider* newMonster = new Spider(id, pos, state);
    _monsters[id] = newMonster;

    std::cout << "[����] ���� ����: ID=" << id
        << " ��ġ(" << pos.x << "," << pos.y << "," << pos.z << ")"
        << " ����: " << static_cast<int>(state) << "\n";

    // Ŭ���̾�Ʈ�� ���� ��Ŷ ����
    sc_packet_monster_spawn pkt;
    pkt.size = sizeof(pkt);
    pkt.type = SC_P_MONSTER_SPAWN;
    pkt.monsterID = id;
    pkt.position = pos;
    pkt.state = state;
    BroadcastToAll(&pkt,-1);
}

//void MonsterManager::DespawnMonster(int64_t id) {
//    std::lock_guard<std::mutex> lock(_mutex);
//
//    auto it = _monsters.find(id);
//    if (it == _monsters.end()) return;
//
//    delete it->second;
//    _monsters.erase(it);
//
//    // Ŭ���̾�Ʈ�� ���� ��Ŷ ����
//    sc_packet_monster_die pkt;
//    pkt.size = sizeof(pkt);
//    pkt.type = SC_P_MONSTER_DIE;
//    pkt.monsterID = id;
//    BroadcastToAll(&pkt);
//}

Spider* MonsterManager::GetMonster(int64_t id) {
    std::lock_guard<std::mutex> lock(_mutex);
    auto it = _monsters.find(id);
    return (it != _monsters.end()) ? it->second : nullptr;
}

std::unordered_map<int64_t, Spider*> MonsterManager::GetAllMonsters() {
    std::lock_guard<std::mutex> lock(_mutex);
    return _monsters;
}

void MonsterManager::UpdateAllMonsters(float deltaTime, const std::vector<XMFLOAT3>& playerPositions) {
    std::lock_guard<std::mutex> lock(_mutex);

    for (auto& pair : _monsters) {
        Spider* monster = pair.second;
        XMFLOAT3 prevPos = monster->GetSpiderPosition();
        uint8_t prevState = monster->GetSpiderAnimaitionState();

        // ���� ����� �÷��̾� ã��
        XMFLOAT3 nearestPlayer = { 0,0,0 };
        float minDist = FLT_MAX;
        for (const auto& pos : playerPositions) {
            float dx = pos.x - prevPos.x;
            float dz = pos.z - prevPos.z;
            float dist = dx * dx + dz * dz;
            if (dist < minDist) {
                minDist = dist;
                nearestPlayer = pos;
            }
        }

        // ���� ������Ʈ
        monster->Update(deltaTime, nearestPlayer);

        // ���� ��ġ/���� ����
        XMFLOAT3 currentPos = monster->GetSpiderPosition();
        uint8_t currentState = monster->GetSpiderAnimaitionState();

        // ���� ��ȭ üũ
        if (memcmp(&prevPos, &currentPos, sizeof(XMFLOAT3)) != 0 || prevState != currentState) {

            sc_packet_monster_move pkt;
            pkt.size = sizeof(pkt);
            pkt.type = SC_P_MONSTER_MOVE;
            pkt.monsterID = monster->GetSpiderID();
            pkt.position = currentPos;
            pkt.state = currentState;

            std::cout << "[����] ���� �̵�: ID=" << pkt.monsterID
                << " �� ��ġ(" << pkt.position.x << "," << pkt.position.z << ")"
                << " ����: " << static_cast<int>(pkt.state) << "\n";

            BroadcastToAll(&pkt, -1);
        }
    }
}
