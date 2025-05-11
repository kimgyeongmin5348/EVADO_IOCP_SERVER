#pragma once

#include "Common.h"

#include <DirectXMath.h>
using namespace DirectX;

//#define SET_DATA_FROM_DATABASE
#define SERVER_STRESS_TEST

#define MAX_PACKET_SIZE 1024
#define SERVER_PORT 3000
#define NUM_WORKER_THREADS 4
#define MAX_USER 5000

#define MAX_BUFFER 8192

constexpr char SC_P_USER_INFO = 1;
constexpr char SC_P_MOVE = 2;
constexpr char SC_P_ENTER = 3;
constexpr char SC_P_LEAVE = 4;
constexpr char CS_P_LOGIN = 5;
constexpr char CS_P_MOVE = 6;
constexpr char SC_P_LOGIN_FAIL = 7;


constexpr unsigned short MAP_HEIGHT = 8;
constexpr unsigned short MAP_WIDTH = 8;

constexpr char SC_P_ITEM_SPAWN = 9;
constexpr char SC_P_ITEM_DESPAWN = 10;
constexpr char SC_P_ITEM_MOVE = 11;
constexpr char CS_P_ITEM_PICKUP = 12;
constexpr char CS_P_ITEM_MOVE = 13;

constexpr char MAX_ID_LENGTH = 20;

constexpr char MOVE_UP = 1;
constexpr char MOVE_DOWN = 2;
constexpr char MOVE_LEFT = 3;
constexpr char MOVE_RIGHT = 4;

constexpr char SC_P_MONSTER_SPAWN = 14;
constexpr char SC_P_MONSTER_MOVE = 15;
constexpr char SC_P_MONSTER_DIE = 16;


#pragma pack (push, 1)

enum class AnimationState : uint8_t {
	IDLE,         // 0
	WALK,         // 1
	RUN,          // 2 
	SWING,        // 3
	JUMP,         // 4
	CROUCH,       // 5
	CROUCH_WALK   // 6
};

struct sc_packet_user_info {
	unsigned char	size;
	char			type;
	long long		id;
	XMFLOAT3		position;
	XMFLOAT3		look;
	XMFLOAT3		right;
	uint8_t			animState;
	//short			hp;
};

struct sc_packet_move {
	unsigned char		size;
	char				type;
	long long			id;
	XMFLOAT3			position;
	XMFLOAT3			look;
	XMFLOAT3			right;
	uint8_t				animState;
};

struct sc_packet_enter {
	unsigned char		size;
	char				type;
	long long			id;
	XMFLOAT3			position;
	XMFLOAT3			look;
	XMFLOAT3			right;
	uint8_t				animState;
};

struct sc_packet_leave {
	unsigned char		size;
	char				type;
	long long			id;
};

struct cs_packet_login {
	unsigned char		size;
	char				type;
	//XMFLOAT3			position;
	char				name[MAX_ID_LENGTH];

};

struct sc_packet_login_fail {
	unsigned char		size;
	char				type;
};

struct cs_packet_move {
	unsigned char		size;
	char				type;
	XMFLOAT3			position;
	XMFLOAT3			look;
	XMFLOAT3			right;
	uint8_t				animState;
};

// 아이템

enum ITEM_TYPE : int {
	ITEM_TYPE_SHOVEL = 1,
	ITEM_TYPE_HANDMAP = 2,
	ITEM_TYPE_FLASHLIGHT = 3,
	ITEM_TYPE_WHISTLE = 4
};

struct sc_packet_item_spawn {
	unsigned char		size;
	char				type;
	long long			item_id;
	XMFLOAT3			position;
	int					item_type;
};

struct sc_packet_item_despawn {
	unsigned char		size;
	char				type;
	long long			item_id;
};

struct cs_packet_item_pickup {
	unsigned char		size;
	char				type;
	long long			item_id;
	long long			player_id;
};

struct cs_packet_item_move {
	unsigned char		size;
	char				type;
	long long			item_id;
	XMFLOAT3			position;
};	

struct sc_packet_item_move {
	unsigned char		size;
	char				type;
	long long			item_id;
	XMFLOAT3			position;
	long long			holder_id; // 소유자 ID (0 = 지면에 있음)
};

// Monster
enum class MonsterAnimationState : uint8_t
{
	IDLE,
	WALK,
	ATTACK
};

struct sc_packet_monster_spawn
{
	unsigned char		size;
	char				type;
	int64_t				monsterID;	
	XMFLOAT3			position;
	uint8_t				state;
};

struct sc_packet_monster_move
{
	unsigned char		size;
	char				type;
	int64_t				monsterID;	
	XMFLOAT3			position;
	uint8_t				state;
};

#pragma pack (pop)

