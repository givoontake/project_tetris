#pragma once
#include "common_packets.h"

#pragma pack(push, 1)

struct S2C_TEST_LOGIN_PACKET {
	PACKET_HEADER header;
	int player_id;
};

struct C2S_TEST_LOGIN_PACKET {
	PACKET_HEADER header;
	int player_id;
	char login_id[MAX_PLAYER_ID_SIZE];
	char login_password[MAX_PLAYER_PASSWORD_SIZE];
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

#pragma pack(pop)
