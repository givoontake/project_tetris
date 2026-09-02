#pragma once
#include <vector>
#include <array>
#include "Player.h"
#include "Session.h"
#include "define_packets.h"
#include "IOCPServer.h"
#include "TetrisRoom.h"

class SingleRoom : public TetrisRoom
{
	std::array<Player, 1> room_players_;

public:
	SingleRoom(IOCPServer* server, PublicRoomInitData data);
	SingleRoom(IOCPServer* server, PrivateRoomInitData data);

	// 공통(오버라이드)
	virtual void HandlePacket(char* packet, const SP<Session>& request_session) override;
	virtual void RemovePlayer(const int player_id) override;
	virtual void SendCreateRoom(const SP<Session>& session) override;
	void StartGame();

	// 싱글 전용
	void CalculateScore(int clear_line_count);
	void RequestUpdateScore();

private:
	virtual std::span<Player> GetRoomPlayers() override;
	virtual void ProcessSpecificRoomTask(const RoomTask& task) override;
	virtual void ProcessGameTick() override;
	void GiveUp(const SP<Session>& request_session);
};

