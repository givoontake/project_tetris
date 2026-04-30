#pragma once
#include <atomic>
#include "settings.h"

#pragma pack(push, 1)

struct S2C_ERROR_PACKET {
	short size;
	char type;
	int error_code;
};

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
	int max_score;
	int win_count;
	int lose_count;
	char nickname[MAX_USER_NAME];
};

struct C2S_LOGIN_PACKET {
	short size;
	char type;
	char login_id[MAX_USER_ID];
	char login_password[MAX_USER_PASSWORD];
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
	long long last_time;
};

struct C2S_DISCONNECT_PACKET {
	short size;
	char type;
};

struct S2C_DISCONNECT_PACKET {
	short size;
	char type;
	int id;
};

struct C2S_ADD_OPEN_ROOM_PACKET {
	short size; // 나중에 방, 게임 등으로 메세지 패킷과 분리한다면 char로 바꿀 수도 있지 않을까..?
	char type;
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

struct C2S_JOIN_OPEN_ROOM_PACKET {
	short size;
	char type;
	int room_id;
};

struct C2S_JOIN_LOCK_ROOM_PACKET {
	short size;
	char type;
	int room_id;
	char room_password[MAX_ROOM_PASSWORD];
};

struct S2C_ADD_USER_PACKET {
	short size;
	char type;
	int id;
	char name[MAX_USER_NAME];
};

struct C2S_DELETE_USER_PACKET {
	short size;
	char type;
	//int id;
};

struct S2C_DELETE_USER_PACKET {
	short size;
	char type;
	int id;
};

struct C2S_READY_PACKET {
	short size;
	char type;
	//int id;
	//bool is_ready;
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
};

struct S2C_SINGLE_START_PACKET {
	short size;
	char type;
	int score; // 이걸 왜 보냈을까?
};

struct S2C_MULTI_START_PACKET {
	short size;
	char type;
};

struct C2S_KICK_PACKET {
	short size;
	char type;
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
	char line_index;
	char combo;
	int score;
	// 클리어될 줄의 인덱스 수에 따라 뒤에 가변으로 붙여 보낸다.
};

struct S2C_ADDLINE_PACKET {
	short size;
	char type;
	int id;
	char hole_x;
};;

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

struct S2C_UPDATE_SCORE_PACKET {
	short size;
	char type;
	int max_score;
};

struct S2C_MATCH_RECORD_PACKET {
	short size;
	char type;
	int win_count;
	int lose_count;
};

struct S2C_UPDATE_HOST_PACKET {
	short size;
	char type;
	int new_host_id;
};

struct C2S_GIVEUP_PACKET {
	short size;
	char type;
};

struct C2S_REQEUST_ROOM_LIST_PACKET {
	short size;
	char type;
};

struct S2C_ROOM_INFO_PACKET {
	short size;
	char type;
	int room_id;
	char room_name[MAX_ROOM_NAME];
	char max_user;
	char cur_user;
	bool is_private;
	bool is_play;
};

struct S2C_INFO_PACKET {
	short size;
	char type;
	int info_code;
};

struct C2S_FAST_MATCHING_PACKET {
	short size;
	char type;
	char max_user;
};

struct C2S_REQUEST_FRIEND_PACKET {
	short size;
	char type;
	int recver_pk; // 누구에게 보내는지
};

struct C2S_DELETE_FRIEND_PACKET {
	short size;
	char type;
	int target_pk;
};

struct C2S_ACCEPT_FRIEND_PACKET {
	short size;
	char type;
	int requester_pk;
};

struct S2C_REQUEST_FRIEND_PACKET {
	short size;
	char type;
	int requester_pk; // 누구에게 왔는지
	char requester_nickname[MAX_USER_NAME];
};

struct S2C_DELETE_FRIEND_PACKET {
	short size;
	char type;
	int target_pk;
};

struct S2C_ADD_FRIEND_PACKET {
	short size;
	char type;
	int friend_id;
	char friend_nickname[MAX_USER_NAME];
};

struct C2S_REQUEST_LOBBY_USER_LIST_PACKET {
	short size;
	char type;
};

struct S2C_LOBBY_USER_INFO_PACKET {
	short size;
	char type;
	int user_pk;
	char nickname[MAX_USER_NAME];
};

struct S2C_FRIEND_INFO_PACKET {
	short size;
	char type;
	int user_pk;
	char nickname[MAX_USER_NAME];
	bool is_lobby; // 세부 상태를 보여주려면 나중에 char로 바꾸기
};

#pragma pack(pop)
