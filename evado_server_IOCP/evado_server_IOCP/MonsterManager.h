#pragma once
#include "WorkerThread.h"
#include "Monster.h"

class SESSION;

class MonsterManager {
public:
    static MonsterManager& GetInstance();

    void SpawnMonster(int64_t id, const XMFLOAT3& pos, uint8_t state);
    void DespawnMonster(int64_t id);
    Spider* GetMonster(int64_t id);
    std::unordered_map<int64_t, Spider*> GetAllMonsters();
    // A* 알고리즘 적용된 것.
    void UpdateAllMonsters(float deltaTime, const std::vector<SESSION*>& playerSessions, const bool map[MAP_HEIGHT][MAP_WIDTH]);
private:
    MonsterManager() = default;
    ~MonsterManager();

    std::unordered_map<int64_t, Spider*> _monsters;
    std::mutex _mutex;
};