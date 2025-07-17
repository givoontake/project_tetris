#pragma once
#include <string>
#include "Tetris.h"
#include "define_room_packet.h"

enum USER_STATE {WAIT, READY};
class RoomSession
{
	Tetris tetris;
	// std::string user_name; // 방 생성할 때 만들도록 일단 하고, 나중에 회원가입 - DB 연동으로 session 클래스에 포함해보자.
	char user_name[MAX_USER_NAME];
	int user_id;
	USER_STATE user_state;

	long long timer;

public:
	RoomSession(C2S_ADD_USER_PACKET& p);
	~RoomSession();

	int GetId() const { return user_id; }
};

