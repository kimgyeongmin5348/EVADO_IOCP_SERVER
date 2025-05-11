#pragma once
#include "Monster.h"


class MonsterManager {
public:
    static MonsterManager& GetInstance();

    void SpawnMonster(int64_t id, const XMFLOAT3& pos, uint8_t state);
    void DespawnMonster(int64_t id);
    Spider* GetMonster(int64_t id);
    std::unordered_map<int64_t, Spider*> GetAllMonsters();
    void UpdateAllMonsters(float deltaTime, const std::vector<XMFLOAT3>& playerPositions);


private:
    MonsterManager() = default;
    ~MonsterManager();

    std::unordered_map<int64_t, Spider*> _monsters;
    std::mutex _mutex;
};