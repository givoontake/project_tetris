#pragma once
#include "common_packets.h"

#pragma pack(push, 1)

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

struct C2S_GIVE_UP_PACKET {
	PACKET_HEADER header;
};

#pragma pack(pop)
