#pragma once
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
#include "TetrisRoom.h"

class MultiRoom : public TetrisRoom
{
	int host_id;
public:
	MultiRoom(IOCPServer* server, Session* session, char max_user, char room_name[MAX_ROOM_NAME], char room_password[MAX_ROOM_PASSWORD]);
	MultiRoom(IOCPServer* server, Session* session, char max_user, char room_name[MAX_ROOM_NAME]);
	~MultiRoom();

	// 공통(오버라이드)
	virtual void HandlePacket(char* packet, Session* request_session) override;
	virtual void BoundPackets(RoomSession& r_session, std::vector<TaskType>& tasks) override;
	virtual void DeleteUser(const int id) override;

	void StartGame(int id);

	int FindNewHost(int delete_id); // 방장이 나갔을 때 새로운 방장 찾기
	void AddUser(Session* new_session);
	void ReadyUser(int id);
	void KickUser(int id, int kick_user_id);
	bool CheckWinner();
};

