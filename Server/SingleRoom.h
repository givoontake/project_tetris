#pragma once
#include <vector>
#include <array>
#include <mutex>
#include "RoomSession.h"
#include "Session.h"
#include "define_packets.h"
#include "RoomPacketHandler.h"
#include "IOCPServer.h"
#include "TetrisRoom.h"

class SingleRoom : public TetrisRoom
{

public:
	SingleRoom(IOCPServer* server, OpenRoomInitData data);
	SingleRoom(IOCPServer* server, LockRoomInitData data);

	// 공통(오버라이드)
	virtual void HandlePacket(char* packet, const SP<Session>& request_session) override;
	virtual void ProcessPlayTasks() override;
	virtual void DeleteUser(const int id) override;
	virtual void SendCreateRoom(const SP<Session>& session) override;
	void StartGame();

	// 싱글 전용
	void CalculateScore(int clear_line_count);
	void RequestUpdateScore();

	void MakeMovePacketData(int move_type);
	void ReduceTimeouts(int type);

private:
	void HandleStartPacket();
	void HandleDeleteUserPacket(const SP<Session>& request_session);
	void HandleMovePacket(char* packet);
	void HandleGiveupPacket();
};

