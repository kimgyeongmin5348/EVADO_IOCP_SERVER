#pragma once

#include "Common.h"

#include <DirectXMath.h>
using namespace DirectX;

//#define SET_DATA_FROM_DATABASE
#define SERVER_STRESS_TEST

#define SERVER_PORT 3000
#define NUM_WORKER_THREADS 4


constexpr char SC_P_USER_INFO = 1;
constexpr char SC_P_MOVE = 2;
constexpr char SC_P_ENTER = 3;
constexpr char SC_P_LEAVE = 4;
constexpr char CS_P_LOGIN = 5;
constexpr char CS_P_MOVE = 6;
constexpr char SC_P_LOGIN_FAIL = 7;

constexpr char MAX_ID_LENGTH = 20;

constexpr char MOVE_UP = 1;
constexpr char MOVE_DOWN = 2;
constexpr char MOVE_LEFT = 3;
constexpr char MOVE_RIGHT = 4;

constexpr unsigned short MAP_HEIGHT = 8;
constexpr unsigned short MAP_WIDTH = 8;

#pragma pack (push, 1)

struct sc_packet_user_info {
	unsigned char	size;
	char			type;
	long long		id;
	//char			name[MAX_ID_LENGTH];  //교수님 코드엔 이게 없음 한번 없에고 해보자...
	XMFLOAT3		position;
	//XMFLOAT3		look;
	//XMFLOAT3		right;
	//short			hp;
};

struct sc_packet_move {
	unsigned char	size;
	char			type;
	long long		id;
	//char			name[MAX_ID_LENGTH];
	XMFLOAT3		position; 
};

struct sc_packet_enter {
	unsigned char	size;
	char			type;
	long long		id;
	char			name[MAX_ID_LENGTH];
	char			o_type;
	XMFLOAT3		position;
};

struct sc_packet_leave {
	unsigned char	size;
	char			type;
	long long		id;
};

struct cs_packet_login {
	unsigned char	size;
	char			type;
	XMFLOAT3		position;
	char			name[MAX_ID_LENGTH];

};

struct sc_packet_login_fail {
	unsigned char	size;
	char			type;
};

struct cs_packet_move {
	unsigned char	size; 
	char			type;
	XMFLOAT3		position; 
};

#pragma pack (pop)

