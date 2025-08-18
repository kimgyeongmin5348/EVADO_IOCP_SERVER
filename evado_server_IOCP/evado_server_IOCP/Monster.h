#pragma once
#include "Common.h"

// [A*]
struct TileCoord {
	int x, y;
	bool operator==(const TileCoord& rhs) const { return x == rhs.x && y == rhs.y; }
};

struct NodeInfo {
	float g = FLT_MAX;      // 현재까지 온 거리
	int parentX = -1, parentY = -1;
	bool closed = false;
};

struct OpenNode {
	int x, y;
	float f; // g + h
	bool operator<(const OpenNode& o) const {
		return f > o.f; // 우선순위큐는 f가 작은게 먼저
	}
};

extern bool worldMap[MAP_HEIGHT][MAP_WIDTH];

class Spider {
public:
	Spider(int64_t id, XMFLOAT3 pos, uint8_t state, int hp);

	bool Update(float fTimeElapsed, const XMFLOAT3& playerPos, const bool map[MAP_HEIGHT][MAP_WIDTH]);

	void SetSpiderPostion(XMFLOAT3 pos) { _position = pos; }
	XMFLOAT3 GetSpiderPosition() const { return _position; }

	int64_t GetSpiderID() const { return _monsterID; }

	void SetSpiderAnimation(uint8_t state) { _state = state; }
	uint8_t GetSpiderAnimaitionState()const { return _state; }

	int GetHP() const { return _hp; }
	void SetHP(int hp) { _hp = hp; }

	void SetRotation(const XMFLOAT3& rot) { _rotation = rot; }
	XMFLOAT3 GetRotation() const { return _rotation; }

	void FindPath(const TileCoord& to, const bool map[MAP_HEIGHT][MAP_WIDTH]);

private:
	int64_t  		_monsterID;
	XMFLOAT3        _position;
	uint8_t			_state;
	float			_attackCooldown = 0.0f;
	int				_hp;
	XMFLOAT3		_rotation;

	TileCoord		_patrolTarget;      // 정처 없이 걷는 목적지
	bool			_hasPatrolTarget = false;
	float			_patrolCooldown = 0.0f;

	TileCoord		_lastTargetTile;
	bool			_hasLastTarget = false;


	std::queue<TileCoord> _path;
	
public:

	

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
