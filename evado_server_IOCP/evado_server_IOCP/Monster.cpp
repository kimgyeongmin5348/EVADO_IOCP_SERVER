#include "Monster.h"

Spider::Spider(int64_t id, XMFLOAT3 pos, uint8_t state)
    : _monsterID(id), _position(pos), _state(state) {}

void Spider::Update(float fTimeElapsed, const XMFLOAT3& playerPos) {
    XMFLOAT3 delta = {
        playerPos.x - _position.x,
        playerPos.y - _position.y,
        playerPos.z - _position.z
    };

    float distance = sqrtf(delta.x * delta.x + delta.y * delta.y + delta.z * delta.z);

    if (distance <= 50.0f) {
        if (distance > 15.0f) {
            // 이동 로직
            _state = static_cast<uint8_t>(MonsterAnimationState::WALK);
            XMFLOAT3 direction = {
                delta.x / distance,
                delta.y / distance,
                delta.z / distance
            };

            _position.x += direction.x * 2.5f * fTimeElapsed;
            _position.y += direction.y * 2.5f * fTimeElapsed;
            _position.z += direction.z * 2.5f * fTimeElapsed;
        }
        else {
            // 공격 로직
            
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