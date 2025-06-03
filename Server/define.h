#pragma once
#include <atomic>
constexpr int BUF_SIZE = 10240;
constexpr int MAX_MESSAGE_SIZE = 512;
constexpr int PORT_NUM = 12345;

constexpr int MAX_USER = 10000;
constexpr int MAX_ARRAY_SIZE = 127;
constexpr int ID_SIZE = 16;

constexpr char S2C_LOGIN = 1;
constexpr char C2S_LOGIN = 2;
constexpr char S2C_MESSAGE = 3;
constexpr char C2S_MESSAGE = 4;
constexpr char S2C_TEST = 5;
constexpr char C2S_TEST = 6;
constexpr char S2C_DISCONNECT = 7;
constexpr char C2S_DISCONNECT = 8;

extern std::atomic<int> remainning_send_IOCP;
extern std::atomic<int> remainning_total_IOCP;
extern std::atomic<int> processed_IOCP;
extern std::atomic<int> user_count;

#pragma pack(push, 1)

struct S2C_LOGIN_PACKET {
	short size;
	char type;
	int id;
};

struct C2S_LOGIN_PACKET {
	short size;
	char type;
	int id;
};

struct S2C_MESSAGE_PACKET {
	short size;
	char type;
	int id;
	char message[MAX_MESSAGE_SIZE];
};

struct C2S_MESSAGE_PACKET {
	short size;
	char type;
	int id;
	char message[MAX_MESSAGE_SIZE];
};

struct S2C_TEST_PACKET {
	short size;
	char type;
	int id;
	short message_size;
	long long last_time;
};

struct C2S_TEST_PACKET {
	short size;
	char type;
	int id; // 테스트 프로그램도 다중 클라이언트를 관리중이므로 필요
	short message_size;
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

#pragma pack(pop)