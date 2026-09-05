#pragma once
#include "common_packets.h"
#include "settings.h"

#pragma pack(push, 1)

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

struct S2C_UPDATE_SCORE_PACKET {
	PACKET_HEADER header;
	int max_score;
};

struct S2C_MATCH_RECORD_PACKET {
	PACKET_HEADER header;
	int win_count;
	int lose_count;
};

struct C2S_ADD_FRIEND_REQUEST_PACKET {
	PACKET_HEADER header;
	int receiver_id;
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
	int requester_id;
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

struct C2S_REQUEST_FRIEND_LIST_PACKET {
	PACKET_HEADER header;
};

struct S2C_FRIEND_INFO_PACKET {
	PACKET_HEADER header;
	int player_id;
	char nickname[MAX_PLAYER_NAME_SIZE];
	bool is_lobby;
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
