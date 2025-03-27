#pragma once
#include <array>
constexpr int BUFFER_SIZE = 4096;
constexpr int ID_SIZE = 16;

constexpr int SEND_SERVER = 2000;
constexpr int SEND_FLASK = 2001;

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
	char message[BUFFER_SIZE];
};

struct C2S_MESSAGE_PACKET {
	char size;
	char type;
	char id[ID_SIZE];
	char message[BUFFER_SIZE];
};