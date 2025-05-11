#pragma once
#include "Common.h"

class Spider {
public:
	Spider(int64_t id, XMFLOAT3 pos, uint8_t state);

	void Update(float fTimeElapsed, const XMFLOAT3& playerPos);  // AI ·ÎÁ÷


	void SetSpiderPostion(XMFLOAT3 pos) { _position = pos; }
	XMFLOAT3 GetSpiderPosition() const { return _position; }
	int64_t GetSpiderID() const { return _monsterID; }
	void SetSpiderAnimation(uint8_t state) { _state = state; }
	uint8_t GetSpiderAnimaitionState()const { return _state; }

private:
	int64_t  		_monsterID;
	XMFLOAT3        _position;
	uint8_t			_state;
};
