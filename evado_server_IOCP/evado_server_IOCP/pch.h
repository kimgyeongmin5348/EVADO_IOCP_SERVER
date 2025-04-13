#pragma once

#include "Common.h"

//#define SET_DATA_FROM_DATABASE
#define SERVER_STRESS_TEST

constexpr char SC_P_AVATAR_INFO = 1;
constexpr char SC_P_MOVE = 2;
constexpr char SC_P_ENTER = 3;
constexpr char SC_P_LEAVE = 4;
constexpr char CS_P_LOGIN = 5;
constexpr char CS_P_MOVE = 6;

constexpr char MAX_ID_LENGTH = 20;

constexpr char MOVE_UP = 1;
constexpr char MOVE_DOWN = 2;
constexpr char MOVE_LEFT = 3;
constexpr char MOVE_RIGHT = 4;

constexpr unsigned short MAP_HEIGHT = 8;
constexpr unsigned short MAP_WIDTH = 8;

#pragma pack (push, 1)

struct sc_packet_user_info {
	unsigned char size;
	char type;
	long long  id;
	short x, y, z;
	short hp;
};

struct sc_packet_move {
	unsigned char size;
	char type;
	long long id;
	short x, y, z;
};

struct sc_packet_enter {
	unsigned char size;
	char type;
	long long  id;
	char name[MAX_ID_LENGTH];
	char o_type;
	short x, y, z;
};

struct sc_packet_leave {
	unsigned char size;
	char type;
	long long  id;
};

struct cs_packet_login {
	unsigned char  size;
	char  type;
	char  name[MAX_ID_LENGTH];
};

struct cs_packet_move {
	unsigned char  size;
	char  type;
	char  direction;
};

#pragma pack (pop)
