#pragma once
#include <array>
#include "MultiRoom.h"

class FivePlayerRoom : public MultiRoom
{
	std::array<Player, 5> room_players_;

public:
	FivePlayerRoom(IOCPServer* server, PublicRoomInitData data);
	FivePlayerRoom(IOCPServer* server, PrivateRoomInitData data);

private:
	virtual std::span<Player> GetRoomPlayers() override;
};
