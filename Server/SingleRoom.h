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

class SingleRoom : public TetrisRoom
{

public:
	SingleRoom(IOCPServer* server, Session* session, char max_user, char room_name[MAX_ROOM_NAME], char room_password[MAX_ROOM_PASSWORD]);
	SingleRoom(IOCPServer* server, Session* session, char max_user, char room_name[MAX_ROOM_NAME]);

	// 공통(오버라이드)
	virtual void HandlePacket(char* packet, Session* request_session) override;
	virtual void BoundPackets(RoomSession& r_session, std::vector<TaskType>& tasks) override;
	virtual void DeleteUser(const int id) override;
	void StartGame();

	// 싱글 전용
	void CalculateScore(RoomSession& r_session, int clear_line_count);
	void RequestUpdateScore(RoomSession& r_session);

	void MakeMovePacketData(RoomSession& r_session, int move_type);
	void ClearEventsInTick();
	void ReduceTimeouts(int type, RoomSession& r_session);
};

