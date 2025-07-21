#pragma once
#include "Common.h"

// 타일좌표 구조체 [A*]
struct TileCoord {
	int x, y;
	bool operator==(const TileCoord& rhs) const { return x == rhs.x && y == rhs.y; }
};


extern bool worldMap[MAP_HEIGHT][MAP_WIDTH];

class Spider {
public:
	Spider(int64_t id, XMFLOAT3 pos, uint8_t state);

	// bool Update(float fTimeElapsed, const XMFLOAT3& playerPos);  // AI 로직
	// **A* 경로사용을 위한 시그니처: map 파라미터 추가**
	bool Update(float fTimeElapsed, const XMFLOAT3& playerPos, const bool map[MAP_HEIGHT][MAP_WIDTH]);

	void SetSpiderPostion(XMFLOAT3 pos) { _position = pos; }
	XMFLOAT3 GetSpiderPosition() const { return _position; }
	int64_t GetSpiderID() const { return _monsterID; }
	void SetSpiderAnimation(uint8_t state) { _state = state; }
	uint8_t GetSpiderAnimaitionState()const { return _state; }

	void FindPath(const TileCoord& to, const bool map[MAP_HEIGHT][MAP_WIDTH]);
	// 테스트용: path size 확인 등 getter

private:
	int64_t  		_monsterID;
	XMFLOAT3        _position;
	uint8_t			_state;
	float			_attackCooldown = 0.0f;

	// **** (추가) A* 경로 저장 ****
	std::queue<TileCoord> _path;

	static TileCoord WorldToTile(float x, float z) {
		return {
			int((x - MAP_ORIGIN_X) / TILE_SIZE),
			int((z - MAP_ORIGIN_Z) / TILE_SIZE)
		};
	}
	static void TileToWorld(int tx, int tz, float& x, float& z) {
		x = MAP_ORIGIN_X + (tx + 0.5f) * TILE_SIZE;
		z = MAP_ORIGIN_Z + (tz + 0.5f) * TILE_SIZE;
	}

};
