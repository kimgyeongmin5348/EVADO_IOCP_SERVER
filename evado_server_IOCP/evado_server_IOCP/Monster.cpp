#include "Monster.h"

Spider::Spider(int64_t id, XMFLOAT3 pos, uint8_t state)
    : _monsterID(id), _position(pos), _state(state) {}


bool Spider::Update(float fTimeElapsed, const XMFLOAT3& playerPos) {

    float dx = playerPos.x - _position.x;
    float dz = playerPos.z - _position.z;
    float distance = sqrtf(dx * dx + dz * dz);

    // 인식 범위
    const float aggroRange = 3.0f;
    // 공격 범위
    const float attackRange = 2.0f;
    // 이동 속도  <- 플레이어 이동속도 보고 설정해보자.
    const float moveSpeed = 2.5f;

    _attackCooldown -= fTimeElapsed;

    if (distance <= aggroRange) {
        if (distance > attackRange) {
            // 추적(이동)
            _state = static_cast<uint8_t>(MonsterAnimationState::WALK);

            float dirX = dx / distance;
            float dirZ = dz / distance;

            _position.x += dirX * moveSpeed * fTimeElapsed;
            _position.z += dirZ * moveSpeed * fTimeElapsed;
            return false;
        }
        else {
            // 공격 로직
            _state = static_cast<uint8_t>(MonsterAnimationState::ATTACK);

            if (_attackCooldown <= 0.0f) {
                _attackCooldown = 1.0f; // 1초 쿨타임
                return true; // 공격 성공!
            }
            std::cout << "[몬스터] : 공격 ! " << '\n';

            
        }
    }
    else {
        _state = static_cast<uint8_t>(MonsterAnimationState::IDLE);
    }
}