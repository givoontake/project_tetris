#pragma once
#include <cstdint>
#include <cstring>
#include "settings.h"

#pragma pack(push, 1)

struct PacketHeader {
	std::uint16_t size;
	std::uint8_t type;
};

constexpr int PACKET_HEADER_SIZE = sizeof(PacketHeader);

struct S2C_ERROR_PACKET {
	PacketHeader header;
	int error_code;
};

struct S2C_TEST_LOGIN_PACKET {
	PacketHeader header;
	int id;
	std::uint64_t client_time;
};

struct C2S_TEST_LOGIN_PACKET {
	PacketHeader header;
	int temp_id;
	std::uint64_t client_time;
};

struct S2C_LOGIN_PACKET {
	PacketHeader header;
	int id;
	int max_score;
	int win_count;
	int lose_count;
	char nickname[MAX_USER_NAME];
};

struct C2S_LOGIN_PACKET {
	PacketHeader header;
	char login_id[MAX_USER_ID];
	char login_password[MAX_USER_PASSWORD];
};

struct S2C_MESSAGE_PACKET {
	PacketHeader header;
	int id;
	char user_name[MAX_USER_NAME];
};

struct C2S_MESSAGE_PACKET {
	PacketHeader header;
};

struct S2C_TEST_PACKET {
	PacketHeader header;
	int id;
	long long last_time;
};

struct C2S_TEST_PACKET {
	PacketHeader header;
	long long last_time;
};

struct C2S_DISCONNECT_PACKET {
	PacketHeader header;
};

struct S2C_DISCONNECT_PACKET {
	PacketHeader header;
	int id;
};

struct C2S_ADD_OPEN_ROOM_PACKET {
	PacketHeader header;
	char max_user;
	char room_name[MAX_ROOM_NAME];
};

struct S2C_ADD_OPEN_ROOM_PACKET {
	PacketHeader header;
	int gen;
	char max_user;
	char room_name[MAX_ROOM_NAME];
};

struct C2S_ADD_LOCK_ROOM_PACKET {
	PacketHeader header;
	char max_user;
	char room_name[MAX_ROOM_NAME];
	char room_password[MAX_ROOM_PASSWORD];
};

struct S2C_ADD_LOCK_ROOM_PACKET {
	PacketHeader header;
	int gen;
	char max_user;
	char room_name[MAX_ROOM_NAME];
	char room_password[MAX_ROOM_PASSWORD];
};

struct C2S_JOIN_OPEN_ROOM_PACKET {
	PacketHeader header;
	int room_gen;
};

struct C2S_JOIN_LOCK_ROOM_PACKET {
	PacketHeader header;
	int room_gen;
	char room_password[MAX_ROOM_PASSWORD];
};

struct S2C_ADD_USER_PACKET {
	PacketHeader header;
	int id;
	char name[MAX_USER_NAME];
};

struct C2S_DELETE_USER_PACKET {
	PacketHeader header;
	//int id;
};

struct S2C_DELETE_USER_PACKET {
	PacketHeader header;
	int id;
};

struct C2S_READY_PACKET {
	PacketHeader header;
	//int id;
	//bool is_ready;
};

struct S2C_READY_PACKET {
	PacketHeader header;
	int id;
	bool is_ready;
};

struct C2S_START_PACKET {
	PacketHeader header;
};

struct S2C_SINGLE_START_PACKET {
	PacketHeader header;
	int score; // ?닿구 ??蹂대깉?꾧퉴?
};

struct S2C_MULTI_START_PACKET {
	PacketHeader header;
};

struct C2S_KICK_PACKET {
	PacketHeader header;
	int kick_user_id;
};

struct S2C_KICK_PACKET {
	PacketHeader header;
	int kick_user_id;
};

struct C2S_MOVE_PACKET {
	PacketHeader header;
	char move_type;
};

struct S2C_MOVE_PACKET {
	PacketHeader header;
	int id;
	char move_type;
};

struct S2C_SPAWN_PACKET {
	PacketHeader header;
	int id;
	char tetromino_type;
	char next_tetromino_type;
	char spawn_x, spawn_y;
};

struct S2C_FIX_PACKET {
	PacketHeader header;
	int id;
	char fixed_x, fixed_y;
};

struct S2C_CLEARLINE_PACKET {
	PacketHeader header;
	int id;
	char line_index;
	char combo;
	int score;
	// ?대━?대맆 以꾩쓽 ?몃뜳???섏뿉 ?곕씪 ?ㅼ뿉 媛蹂?쇰줈 遺숈뿬 蹂대궦??
};

struct S2C_ADDLINE_PACKET {
	PacketHeader header;
	int id;
	char hole_x;
};;

struct S2C_GAMEOVER_PACKET {
	PacketHeader header;
	int id;
};

struct S2C_GAMEEND_PACKET {
	PacketHeader header;
	int winner_id;
};

struct S2C_UPDATE_SCORE_PACKET {
	PacketHeader header;
	int max_score;
};

struct S2C_MATCH_RECORD_PACKET {
	PacketHeader header;
	int win_count;
	int lose_count;
};

struct S2C_UPDATE_HOST_PACKET {
	PacketHeader header;
	int new_host_id;
};

struct C2S_GIVEUP_PACKET {
	PacketHeader header;
};

struct C2S_REQUEST_ROOM_LIST_PACKET {
	PacketHeader header;
};

struct S2C_ROOM_INFO_PACKET {
	PacketHeader header;
	int room_gen;
	char room_name[MAX_ROOM_NAME];
	char max_user;
	char cur_user;
	bool is_private;
	bool is_play;
};

struct S2C_INFO_PACKET {
	PacketHeader header;
	int info_code;
};

struct C2S_FAST_MATCHING_PACKET {
	PacketHeader header;
	char max_user;
};

struct C2S_REQUEST_FRIEND_PACKET {
	PacketHeader header;
	int recver_id; // ?꾧뎄?먭쾶 蹂대궡?붿?
};

struct C2S_DELETE_FRIEND_PACKET {
	PacketHeader header;
	int target_id;
};

struct C2S_ACCEPT_FRIEND_PACKET {
	PacketHeader header;
	int requester_id;
};

struct S2C_REQUEST_FRIEND_PACKET {
	PacketHeader header;
	int requester_id; // ?꾧뎄?먭쾶 ?붾뒗吏
	char requester_nickname[MAX_USER_NAME];
};

struct S2C_DELETE_FRIEND_PACKET {
	PacketHeader header;
	int target_id;
};

struct S2C_ADD_FRIEND_PACKET {
	PacketHeader header;
	int friend_id;
	char friend_nickname[MAX_USER_NAME];
};

struct C2S_REQUEST_LOBBY_USER_LIST_PACKET {
	PacketHeader header;
};

struct S2C_LOBBY_USER_INFO_PACKET {
	PacketHeader header;
	int user_id;
	char nickname[MAX_USER_NAME];
};

struct C2S_REQUEST_FRIEND_LIST_PACKET {
	PacketHeader header;
};

struct S2C_FRIEND_INFO_PACKET {
	PacketHeader header;
	int user_id;
	char nickname[MAX_USER_NAME];
	bool is_lobby; // ?몃? ?곹깭瑜?蹂댁뿬二쇰젮硫??섏쨷??char濡?諛붽씀湲?
};

struct C2S_REQUEST_RANKING_PACKET {
	PacketHeader header;
};

struct S2C_RANKING_INFO_PACKET {
	PacketHeader header;
	char nickname[MAX_USER_NAME];
	int score;
};

#pragma pack(pop)

