#pragma once
#include <atomic>
constexpr int BUF_SIZE = 10240;
constexpr int MAX_MESSAGE_SIZE = 512;
constexpr int PORT_NUM = 12345;

constexpr int MAX_USER = 10000;
constexpr int MAX_ROOM = 5000;
constexpr int MAX_ROOM_NAME = 48; // 16자 * UTF-8 1문자 크기(3)
constexpr int MAX_ROOM_PASSWORD = 48;
constexpr int MAX_ARRAY_SIZE = 127;
constexpr int ID_SIZE = 16;

constexpr int MAX_USER_ID = 48;
constexpr int MAX_USER_PASSWORD = 48;
constexpr int MAX_USER_NAME = 48;

#pragma pack(push, 1)

struct S2C_TEST_LOGIN_PACKET {
	short size;
	char type;
	int id;
};

struct C2S_TEST_LOGIN_PACKET {
	short size;
	char type;
	int temp_id;
};

struct S2C_LOGIN_PACKET {
	short size;
	char type;
	int id;
	char user_name[MAX_USER_NAME];
};

struct C2S_LOGIN_PACKET {
	short size;
	char type;
	char user_id[MAX_USER_ID];
	char user_password[MAX_USER_PASSWORD];
};

struct S2C_MESSAGE_PACKET {
	short size;
	char type;
	int id;
	char user_name[MAX_USER_NAME];
};

struct C2S_MESSAGE_PACKET {
	short size;
	char type;
	int id;
};

struct S2C_TEST_PACKET {
	short size;
	char type;
	int id;
	long long last_time;
};

struct C2S_TEST_PACKET {
	short size;
	char type;
	int id; // 테스트 프로그램도 다중 클라이언트를 관리중이므로 필요
	long long last_time;
};

struct C2S_DISCONNECT_PACKET {
	short size;
	char type;
	int id;
};

struct S2C_DISCONNECT_PACKET {
	short size;
	char type;
	int id;
};

struct C2S_ADD_OPEN_ROOM_PACKET {
	short size; // 나중에 방, 게임 등으로 메세지 패킷과 분리한다면 char로 바꿀 수도 있지 않을까..?
	char type;
	int id;
	char max_user;
	char room_name[MAX_ROOM_NAME];
};

struct S2C_ADD_OPEN_ROOM_PACKET {
	short size;
	char type;
	int id;
	char max_user;
	char room_name[MAX_ROOM_NAME];
};

struct C2S_ADD_LOCK_ROOM_PACKET {
	short size; 
	char type;
	int id;
	char max_user;
	char room_name[MAX_ROOM_NAME];
	char room_password[MAX_ROOM_PASSWORD];
};

struct S2C_ADD_LOCK_ROOM_PACKET {
	short size; 
	char type;
	int id;
	char max_user;
	char room_name[MAX_ROOM_NAME];
	char room_password[MAX_ROOM_PASSWORD];
};

struct C2S_ADD_USER_PACKET {
	short size;
	char type;
	int id;
	int room_id;
	char name[MAX_USER_NAME];
};

struct S2C_ADD_USER_PACKET {
	short size;
	char type;
	int id;
	bool is_add;
	char name[MAX_USER_NAME];
};

struct C2S_DELETE_USER_PACKET {
	short size;
	char type;
	int id;
};

struct S2C_DELETE_USER_PACKET {
	short size;
	char type;
	int id;
	int new_host_id;
};

struct C2S_READY_PACKET {
	short size;
	char type;
	int id;
	bool is_ready;
};

struct S2C_READY_PACKET {
	short size;
	char type;
	int id;
	bool is_ready;
};

struct C2S_START_PACKET {
	short size;
	char type;
	int id;
};

struct S2C_START_PACKET {
	short size;
	char type;
	bool is_start;
};

struct C2S_KICK_PACKET {
	short size;
	char type;
	int id;
	int kick_user_id;
};

struct S2C_KICK_PACKET {
	short size;
	char type;
	int kick_user_id;
};

struct C2S_MOVE_PACKET {
	short size;
	char type;
	char move_type;
};

struct S2C_MOVE_PACKET {
	short size;
	char type;
	int id;
	char move_type;
};

struct S2C_SPAWN_PACKET {
	short size;
	char type;
	int id;
	char tetromino_type;
	char next_tetromino_type;
	char spawn_x, spawn_y;
};

struct S2C_FIX_PACKET {
	short size;
	char type;
	int id;
	char fixed_x, fixed_y;
};

struct S2C_CLEARLINE_PACKET {
	short size;
	char type;
	int id;
	// 클리어될 줄의 인덱스 수에 따라 뒤에 가변으로 붙여 보낸다.
};

struct S2C_GAMEOVER_PACKET {
	short size;
	char type;
	int id;
};

struct S2C_GAMEEND_PACKET {
	short size;
	char type;
	int winner_id;
};
#pragma pack(pop)
