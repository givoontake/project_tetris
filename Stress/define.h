#pragma once
constexpr int BUF_SIZE = 10240;
constexpr int MAX_MESSAGE_SIZE = 128;
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

constexpr char S2C_TEST_LOGIN = 101;
constexpr char C2S_TEST_LOGIN = 102;

#pragma pack(push, 1)

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
	long long last_time;
};

struct C2S_TEST_PACKET {
	short size;
	char type;
	int id; 
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