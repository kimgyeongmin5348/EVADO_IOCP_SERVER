#include "Monster.h"

Spider::Spider(int64_t id, XMFLOAT3 pos, uint8_t state)
    : _monsterID(id), _position(pos), _state(state) {}


void Spider::Update(float fTimeElapsed, const XMFLOAT3& playerPos) {

    float dx = playerPos.x - _position.x;
    float dz = playerPos.z - _position.z;
    float distance = sqrtf(dx * dx + dz * dz);

    // 인식 범위
    const float aggroRange = 50.0f;
    // 공격 범위
    const float attackRange = 3.0f;
    // 이동 속도
    const float moveSpeed = 2.5f;


    if (distance <= aggroRange) {
        if (distance > attackRange) {
            // 추적(이동)
            _state = static_cast<uint8_t>(MonsterAnimationState::WALK);

            float dirX = dx / distance;
            float dirZ = dz / distance;

            _position.x += dirX * moveSpeed * fTimeElapsed;
            _position.z += dirZ * moveSpeed * fTimeElapsed;
        }
        else {
            // 공격 로직
            std::cout << "[몬스터] : 공격 ! " << '\n';

            //_state = static_cast<uint8_t>(MonsterAnimationState::ATTACK);
            //_attackCooldown -= fTimeElapsed;
            //if (_attackCooldown <= 0.0f) {
            //    // 실제 공격 처리 로직 (데미지 계산 등)
            //    _attackCooldown = 1.0f;  // 1초마다 공격
            //}
        }
    }
    else {
        _state = static_cast<uint8_t>(MonsterAnimationState::IDLE);
    }
}