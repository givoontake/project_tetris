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
	SingleRoom(IOCPServer* server, Session* session, OpenRoomInitData data);
	SingleRoom(IOCPServer* server, Session* session, LockRoomInitData data);

	// 공통(오버라이드)
	virtual void HandlePacket(char* packet, Session* request_session) override;
	virtual void ProcessPlayTasks() override;
	virtual void DeleteUser(const int id) override;
	void StartGame();

	// 싱글 전용
	void CalculateScore(int clear_line_count);
	void RequestUpdateScore();

	void MakeMovePacketData(int move_type);
	void ReduceTimeouts(int type);
};

