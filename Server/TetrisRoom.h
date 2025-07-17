#pragma once
#include <vector>
#include <mutex>
#include "RoomSession.h"
#include "define_room_packet.h"
#include "define.h"

enum ROOM_STATE {EMPTY, LOBBY, PLAY};

class TetrisRoom
{
	std::vector<RoomSession> users; 
	ROOM_STATE room_state;
	int room_id; // 있으면 나중에 순회할 때 편할 것 같은 느낌이 드는데..
	int host_id;
	char room_name[MAX_ROOM_NAME];
	int max_user;

	std::mutex room_mutex;
	
public:
	TetrisRoom();
	~TetrisRoom();

	void AddUser(C2S_ADD_USER_PACKET& p);
	void DeleteUser(C2S_DELETE_USER_PACKET& p);
	void InitRoom(const char* name, int user_id, int _max_user);
};

