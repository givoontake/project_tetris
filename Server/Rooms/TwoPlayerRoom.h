#pragma once
#include <array>
#include "MultiRoom.h"

class TwoPlayerRoom : public MultiRoom
{
	std::array<Player, 2> room_players_;

public:
	TwoPlayerRoom(TetrisServer* server, PublicRoomInitData data);
	TwoPlayerRoom(TetrisServer* server, PrivateRoomInitData data);

private:
	virtual std::span<Player> GetRoomPlayers() override;
};
