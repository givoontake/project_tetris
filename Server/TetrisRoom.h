#pragma once
#include <vector>
#include <array>
#include <mutex>
#include "RoomSession.h"
#include "Session.h"
#include "define.h"
#include "packetType.h"
#include "RoomPacketHandler.h"
#include "IOCPServer.h"

enum ROOM_STATE {EMPTY, WAIT, PLAY};

class TetrisRoom
{
	//std::array<RoomSession*, MAX_USER>& users;
	std::vector<RoomSession> room_users; // 아토믹 변수는 복사가 안돼서..
	RoomPacketHandler room_handler;
	IOCPServer* server;
	Atomic<ROOM_STATE> room_state;

	int host_id;
	int room_id;
	char room_name[MAX_ROOM_NAME];
	bool is_password;
	char room_password[MAX_ROOM_PASSWORD];
	char max_user;

	//std::mutex room_mutex;
	
public:
	TetrisRoom(IOCPServer* server);
	~TetrisRoom();

	ROOM_STATE GetRoomState() const { return room_state.GetSelf(); }
	RoomPacketHandler GetRoomPacketHandler() const { return room_handler; }

	void SetRoomId(const int room_index);
	void SetRoomState(const ROOM_STATE new_state); // 방 상태 변경은 딱히 동시접근할 일이 없어보임
	int FindNewHost(); // 방장이 나갔을 때 새로운 방장 찾기

	void InitRoom(char* packet, Session* session);
	void AddUser(Session* new_session);
	void DeleteUser(const int id);
	void ReadyUser(int id);
	void KickUser(int id, int kick_user_id);
	void StartGame(const int id);
	void Broadcast(char* packet, const HANDLE iocp_handle);
	void SendToSelf(char* packet, Session* session);

	void InitGame();
	//void SendToSelf(char* packet, Session* session);
};
