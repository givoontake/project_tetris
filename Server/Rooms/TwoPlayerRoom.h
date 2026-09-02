#pragma once
#include <array>
#include "MultiRoom.h"

class TwoPlayerRoom : public MultiRoom
{
	std::array<Player, 2> room_players_;

public:
	TwoPlayerRoom(IOCPServer* server, PublicRoomInitData data);
	TwoPlayerRoom(IOCPServer* server, PrivateRoomInitData data);

private:
	virtual std::span<Player> GetRoomPlayers() override;
};
