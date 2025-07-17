#pragma once
constexpr int MAX_USER_NAME = 48;

// 주고받는 패킷 형태가 같으면 굳이 나누지 않을 예정

struct C2S_ADD_USER_PACKET {
	short size;
	char type;
	int id;
	char name[MAX_USER_NAME];
};

struct S2C_ADD_USER_PACKET {
	short size;
	char type;
	int id;
	int host_id;
	char name[MAX_USER_NAME];
};

struct C2S_DELETE_USER_PACKET {
	short size;
	char type;
	int id;
};

struct S2C_DELETE_USER_PACKET {
	short size;
	char type;
	int id;
	int new_host_id;
};