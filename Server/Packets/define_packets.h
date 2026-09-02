#pragma once
#include <cstdint>
#include <cstring>
#include "settings.h"

#pragma pack(push, 1)

struct PACKET_HEADER {
	std::uint16_t size;
	std::uint8_t type;
};

constexpr int PACKET_HEADER_SIZE = sizeof(PACKET_HEADER);

struct S2C_ERROR_PACKET {
	PACKET_HEADER header;
	int error_code;
};

struct S2C_TEST_LOGIN_PACKET {
	PACKET_HEADER header;
	int player_id;
};

struct C2S_TEST_LOGIN_PACKET {
	PACKET_HEADER header;
	int temp_id;
};

struct S2C_LOGIN_PACKET {
	PACKET_HEADER header;
	int player_id;
	int max_score;
	int win_count;
	int lose_count;
	char nickname[MAX_PLAYER_NAME_SIZE];
};

struct C2S_LOGIN_PACKET {
	PACKET_HEADER header;
	char login_id[MAX_PLAYER_ID_SIZE];
	char login_password[MAX_PLAYER_PASSWORD_SIZE];
};

struct S2C_MESSAGE_PACKET {
	PACKET_HEADER header;
	int player_id;
	char nickname[MAX_PLAYER_NAME_SIZE];
};

struct C2S_MESSAGE_PACKET {
	PACKET_HEADER header;
};

struct S2C_TEST_PACKET {
	PACKET_HEADER header;
	int player_id;
	long long last_time;
};

struct C2S_TEST_PACKET {
	PACKET_HEADER header;
	long long last_time;
};

struct C2S_DISCONNECT_PACKET {
	PACKET_HEADER header;
};

struct S2C_DISCONNECT_PACKET {
	PACKET_HEADER header;
	int player_id;
};

struct C2S_ADD_PUBLIC_ROOM_PACKET {
	PACKET_HEADER header;
	char max_player_count;
	char room_name[MAX_ROOM_NAME_SIZE];
};

struct S2C_ADD_PUBLIC_ROOM_PACKET {
	PACKET_HEADER header;
	int room_gen;
	char max_player_count;
	char room_name[MAX_ROOM_NAME_SIZE];
};

struct C2S_ADD_PRIVATE_ROOM_PACKET {
	PACKET_HEADER header;
	char max_player_count;
	char room_name[MAX_ROOM_NAME_SIZE];
	char room_password[MAX_ROOM_PASSWORD_SIZE];
};

struct S2C_ADD_PRIVATE_ROOM_PACKET {
	PACKET_HEADER header;
	int room_gen;
	char max_player_count;
	char room_name[MAX_ROOM_NAME_SIZE];
	char room_password[MAX_ROOM_PASSWORD_SIZE];
};

struct C2S_JOIN_PUBLIC_ROOM_PACKET {
	PACKET_HEADER header;
	int room_gen;
};

struct C2S_JOIN_PRIVATE_ROOM_PACKET {
	PACKET_HEADER header;
	int room_gen;
	char room_password[MAX_ROOM_PASSWORD_SIZE];
};

struct S2C_ADD_PLAYER_PACKET {
	PACKET_HEADER header;
	int player_id;
	char nickname[MAX_PLAYER_NAME_SIZE];
};

struct C2S_REMOVE_PLAYER_PACKET {
	PACKET_HEADER header;
	//int id;
};

struct S2C_REMOVE_PLAYER_PACKET {
	PACKET_HEADER header;
	int player_id;
};

struct C2S_READY_PACKET {
	PACKET_HEADER header;
	//int id;
	//bool is_ready;
};

struct S2C_READY_PACKET {
	PACKET_HEADER header;
	int player_id;
	bool is_ready;
};

struct C2S_START_PACKET {
	PACKET_HEADER header;
};

struct S2C_SINGLE_START_PACKET {
	PACKET_HEADER header;
	int score; // ?닿구 ??蹂대깉?꾧퉴?
};

struct S2C_MULTI_START_PACKET {
	PACKET_HEADER header;
};

struct C2S_KICK_PACKET {
	PACKET_HEADER header;
	int kick_player_id;
};

struct S2C_KICK_PACKET {
	PACKET_HEADER header;
	int kick_player_id;
};

struct C2S_MOVE_PACKET {
	PACKET_HEADER header;
	char move_type;
};

struct S2C_MOVE_PACKET {
	PACKET_HEADER header;
	int player_id;
	char move_type;
};

struct S2C_SPAWN_PACKET {
	PACKET_HEADER header;
	int player_id;
	char tetromino_type;
	char next_tetromino_type;
	char spawn_x, spawn_y;
};

struct S2C_FIX_PACKET {
	PACKET_HEADER header;
	int player_id;
	char fixed_x, fixed_y;
};

struct S2C_CLEAR_LINE_PACKET {
	PACKET_HEADER header;
	int player_id;
	char line_index;
	char combo;
	int score;
	// ?대━?대맆 以꾩쓽 ?몃뜳???섏뿉 ?곕씪 ?ㅼ뿉 媛蹂?쇰줈 遺숈뿬 蹂대궦??
};

struct S2C_ADD_LINE_PACKET {
	PACKET_HEADER header;
	int player_id;
	char hole_x;
};;

struct S2C_GAME_OVER_PACKET {
	PACKET_HEADER header;
	int player_id;
};

struct S2C_GAME_END_PACKET {
	PACKET_HEADER header;
	int winner_id;
};

struct S2C_UPDATE_SCORE_PACKET {
	PACKET_HEADER header;
	int max_score;
};

struct S2C_MATCH_RECORD_PACKET {
	PACKET_HEADER header;
	int win_count;
	int lose_count;
};

struct S2C_UPDATE_HOST_PACKET {
	PACKET_HEADER header;
	int new_host_id;
};

struct C2S_GIVE_UP_PACKET {
	PACKET_HEADER header;
};

struct C2S_REQUEST_ROOM_LIST_PACKET {
	PACKET_HEADER header;
};

struct S2C_ROOM_INFO_PACKET {
	PACKET_HEADER header;
	int room_gen;
	char room_name[MAX_ROOM_NAME_SIZE];
	char max_player_count;
	char current_player_count;
	bool is_private;
	bool is_play;
};

struct S2C_INFO_PACKET {
	PACKET_HEADER header;
	int info_code;
};

struct C2S_FAST_MATCHING_PACKET {
	PACKET_HEADER header;
	char max_player_count;
};

struct C2S_ADD_FRIEND_REQUEST_PACKET {
	PACKET_HEADER header;
	int receiver_id; // ?꾧뎄?먭쾶 蹂대궡?붿?
};

struct C2S_DELETE_FRIEND_PACKET {
	PACKET_HEADER header;
	int target_id;
};

struct C2S_ACCEPT_FRIEND_PACKET {
	PACKET_HEADER header;
	int requester_id;
};

struct S2C_ADD_FRIEND_REQUEST_PACKET {
	PACKET_HEADER header;
	int requester_id; // ?꾧뎄?먭쾶 ?붾뒗吏
	char requester_nickname[MAX_PLAYER_NAME_SIZE];
};

struct S2C_DELETE_FRIEND_PACKET {
	PACKET_HEADER header;
	int target_id;
};

struct S2C_ADD_FRIEND_PACKET {
	PACKET_HEADER header;
	int friend_id;
	char friend_nickname[MAX_PLAYER_NAME_SIZE];
};

struct C2S_REQUEST_LOBBY_PLAYER_LIST_PACKET {
	PACKET_HEADER header;
};

struct S2C_LOBBY_PLAYER_INFO_PACKET {
	PACKET_HEADER header;
	int player_id;
	char nickname[MAX_PLAYER_NAME_SIZE];
};

struct C2S_REQUEST_FRIEND_LIST_PACKET {
	PACKET_HEADER header;
};

struct S2C_FRIEND_INFO_PACKET {
	PACKET_HEADER header;
	int player_id;
	char nickname[MAX_PLAYER_NAME_SIZE];
	bool is_lobby; // ?몃? ?곹깭瑜?蹂댁뿬二쇰젮硫??섏쨷??char濡?諛붽씀湲?
};

struct C2S_REQUEST_RANKINGS_PACKET {
	PACKET_HEADER header;
};

struct S2C_RANKING_INFO_PACKET {
	PACKET_HEADER header;
	char nickname[MAX_PLAYER_NAME_SIZE];
	int score;
};

#pragma pack(pop)

