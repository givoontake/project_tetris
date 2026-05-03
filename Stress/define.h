#pragma once
#include <cstdint>

constexpr int BUF_SIZE = 2048;
constexpr int MAX_MESSAGE_SIZE = 128;
constexpr int PORT_NUM = 12345;
constexpr int STRESS_VIEW_PORT = 23456;

constexpr int MAX_USER = 30000;
constexpr int STRESS_SESSION_COUNT = 30000;
constexpr int STRESS_WORKER_THREAD_COUNT = 4;
constexpr int STRESS_MOVE_INTERVAL_MS = 100;
constexpr int STRESS_LATENCY_AVERAGE_COUNT = 10000;

constexpr int MAX_USER_ID = 48;
constexpr int MAX_USER_PASSWORD = 48;

constexpr std::uint8_t S2C_ERROR = 0;
constexpr std::uint8_t S2C_LOGIN = 1;
constexpr std::uint8_t C2S_LOGIN = 2;
constexpr std::uint8_t S2C_TEST = 5;
constexpr std::uint8_t C2S_TEST = 6;
constexpr std::uint8_t S2C_DISCONNECT = 7;
constexpr std::uint8_t C2S_DISCONNECT = 8;
constexpr std::uint8_t S2C_SINGLE_START = 20;
constexpr std::uint8_t C2S_MOVE = 23;
constexpr std::uint8_t S2C_MULTI_START = 39;
constexpr std::uint8_t S2C_TEST_LOGIN = 101;
constexpr std::uint8_t C2S_TEST_LOGIN = 102;
constexpr std::uint8_t C2S_STRESS_ENTER_MATCH = 103;
constexpr std::uint8_t C2S_TEST_MOVE = 104;
constexpr std::uint8_t S2C_TEST_MOVE = 105;
constexpr std::uint8_t S2V_STRESS_METRICS = 2;
constexpr std::uint8_t V2S_STRESS_CONNECT_CONTROL = 3;

constexpr char STRESS_LOGIN_ID[MAX_USER_ID] = "tester";
constexpr char STRESS_LOGIN_PASSWORD[MAX_USER_PASSWORD] = "1234";

#pragma pack(push, 1)

struct PacketHeader {
	std::uint16_t size;
	std::uint8_t type;
};

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
};

struct C2S_LOGIN_PACKET {
	PacketHeader header;
	char login_id[MAX_USER_ID];
	char login_password[MAX_USER_PASSWORD];
};

struct C2S_STRESS_ENTER_MATCH_PACKET {
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

struct C2S_TEST_MOVE_PACKET {
	PacketHeader header;
	char move_type;
	std::uint32_t sequence;
	std::uint64_t client_time;
};

struct S2C_TEST_MOVE_PACKET {
	PacketHeader header;
	int id;
	std::uint32_t sequence;
	std::uint64_t client_time;
};

struct StressMetricsSnapshot {
	std::uint64_t connected_client;
	std::uint64_t playing_client;
	std::uint64_t current_latency_ms;
	std::uint64_t average_latency_ms;
	std::uint64_t max_latency_ms;
	std::uint64_t current_login_latency_ms;
	std::uint64_t average_login_latency_ms;
	std::uint64_t max_login_latency_ms;
	std::uint64_t connect_enabled;
};

struct S2V_STRESS_METRICS_PACKET {
	PacketHeader header;
	StressMetricsSnapshot metrics;
};

struct V2S_STRESS_CONNECT_CONTROL_PACKET {
	PacketHeader header;
	std::uint8_t connect_enabled;
};

struct C2S_DISCONNECT_PACKET {
	PacketHeader header;
};

struct S2C_DISCONNECT_PACKET {
	PacketHeader header;
	int id;
};

#pragma pack(pop)
