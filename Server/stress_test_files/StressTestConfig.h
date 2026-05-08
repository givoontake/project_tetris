#pragma once
#include <cstdint>
#include "../define_packets.h"

enum class SERVER_RUN_MODE
{
	NORMAL,
	STRESS_TEST
};

constexpr SERVER_RUN_MODE SERVER_MODE = SERVER_RUN_MODE::NORMAL;
constexpr int STRESS_TEST_ROOM_USER_COUNT = 5;
constexpr int STRESS_TEST_USER_ID_START = 1000000;

#pragma pack(push, 1)

struct C2S_STRESS_ENTER_MATCH_PACKET
{
	PacketHeader header;
};

struct C2S_TEST_MOVE_PACKET
{
	PacketHeader header;
	char move_type;
	std::uint32_t sequence;
	std::uint64_t client_time;
};

struct S2C_TEST_MOVE_PACKET
{
	PacketHeader header;
	int id;
	std::uint32_t sequence;
	std::uint64_t client_time;
};

#pragma pack(pop)
