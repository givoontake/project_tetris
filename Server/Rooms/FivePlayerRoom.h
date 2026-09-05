#pragma once
#include <array>
#include "MultiRoom.h"

class FivePlayerRoom : public MultiRoom
{
	std::array<Player, 5> room_players_;

public:
	FivePlayerRoom(TetrisServer* server, PublicRoomInitData data);
	FivePlayerRoom(TetrisServer* server, PrivateRoomInitData data);

private:
	virtual std::span<Player> GetRoomPlayers() override;
};
