#pragma once
constexpr int BUF_SIZE = 1024;
constexpr int PORT_NUM = 12345;

constexpr int MAX_USER = 1000;
constexpr int MAX_ARRAY_SIZE = 127;
constexpr int ID_SIZE = 16;

constexpr char S2C_LOGIN = 1;
constexpr char C2S_LOGIN = 2;
constexpr char S2C_MESSAGE = 3;
constexpr char C2S_MESSAGE = 4;

struct S2C_LOGIN_PACKET {
	char size;
	char type;
	char id[ID_SIZE];
};

struct C2S_LOGIN_PACKET {
	char size;
	char type;
	char id[ID_SIZE];
};

struct S2C_MESSAGE_PACKET {
	char size;
	char type;
	char id[ID_SIZE];
	char message[BUF_SIZE];
};

struct C2S_MESSAGE_PACKET {
	char size;
	char type;
	char id[ID_SIZE];
	char message[BUF_SIZE];
};