#pragma once
constexpr int BUFFER_SIZE = 1024;
constexpr int ID_SIZE = 16;

constexpr char S2C_LOGIN = 1;
constexpr char S2C_MESSAGE = 2;

constexpr char C2S_LOGIN = 11;
constexpr char C2S_MESSAGE = 12;

struct S2C_LOGIN_PACKET {
	char size;
	char type;
	char id[ID_SIZE];
};

struct C2S_LOGIN_SIZE {
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