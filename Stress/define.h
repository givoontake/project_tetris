#pragma once
#include <cstdint>

constexpr int BUF_SIZE = 2048;
constexpr int PORT_NUM = 12345;
constexpr int MAX_USER = 10000;
constexpr int ROOM_PLAYER_COUNT = 5;
constexpr int ROOM_COUNT = MAX_USER / ROOM_PLAYER_COUNT;
constexpr int IO_THREAD_COUNT = 4;
constexpr int CONNECT_THREAD_COUNT = 2;
constexpr int SEND_THREAD_COUNT = 10;
constexpr int CONNECT_INFLIGHT_COUNT = 256;
constexpr int MOVE_INTERVAL_MS = 100;
constexpr int MEASUREMENT_SECONDS = 30;
constexpr int MAX_ROOM_NAME_SIZE = 48;
constexpr int MAX_PLAYER_ID_SIZE = 48;
constexpr int MAX_PLAYER_PASSWORD_SIZE = 48;
constexpr int MOVE_TIME_QUEUE_SIZE = 64;

using RoomKey = std::uint64_t;

constexpr std::uint8_t S2C_ERROR = 0;
constexpr std::uint8_t S2C_DISCONNECT = 7;
constexpr std::uint8_t C2S_DISCONNECT = 8;
constexpr std::uint8_t C2S_ADD_PUBLIC_ROOM = 9;
constexpr std::uint8_t S2C_ADD_PUBLIC_ROOM = 10;
constexpr std::uint8_t C2S_JOIN_PUBLIC_ROOM = 13;
constexpr std::uint8_t S2C_ADD_PLAYER = 14;
constexpr std::uint8_t C2S_READY = 17;
constexpr std::uint8_t S2C_READY = 18;
constexpr std::uint8_t C2S_START = 19;
constexpr std::uint8_t C2S_MOVE = 23;
constexpr std::uint8_t S2C_MOVE = 24;
constexpr std::uint8_t S2C_GAME_END = 29;
constexpr std::uint8_t S2C_MULTI_START = 39;
constexpr std::uint8_t S2C_TEST_LOGIN = 101;
constexpr std::uint8_t C2S_TEST_LOGIN = 102;

#pragma pack(push, 1)

struct PacketHeader
{
	std::uint16_t size;
	std::uint8_t type;
};

struct S2C_ERROR_PACKET
{
	PacketHeader header;
	int error_code;
};

struct C2S_TEST_LOGIN_PACKET
{
	PacketHeader header;
	int player_id;
	char login_id[MAX_PLAYER_ID_SIZE];
	char login_password[MAX_PLAYER_PASSWORD_SIZE];
};

struct S2C_TEST_LOGIN_PACKET
{
	PacketHeader header;
	int player_id;
};

struct C2S_ADD_PUBLIC_ROOM_PACKET
{
	PacketHeader header;
	char max_player_count;
	char room_name[MAX_ROOM_NAME_SIZE];
};

struct S2C_ADD_PUBLIC_ROOM_PACKET
{
	PacketHeader header;
	RoomKey room_key;
	char max_player_count;
	char room_name[MAX_ROOM_NAME_SIZE];
};

struct C2S_JOIN_PUBLIC_ROOM_PACKET
{
	PacketHeader header;
	RoomKey room_key;
};

struct C2S_READY_PACKET
{
	PacketHeader header;
};

struct S2C_READY_PACKET
{
	PacketHeader header;
	int player_id;
	bool is_ready;
};

struct C2S_START_PACKET
{
	PacketHeader header;
};

struct C2S_MOVE_PACKET
{
	PacketHeader header;
	char move_type;
};

struct S2C_MOVE_PACKET
{
	PacketHeader header;
	int player_id;
	char move_type;
};

struct C2S_DISCONNECT_PACKET
{
	PacketHeader header;
};

#pragma pack(pop)
