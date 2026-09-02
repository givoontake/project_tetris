#include "FivePlayerRoom.h"

FivePlayerRoom::FivePlayerRoom(IOCPServer* server, PublicRoomInitData data)
	: MultiRoom(server, data)
{
	max_player_count_ = 5;
}

FivePlayerRoom::FivePlayerRoom(IOCPServer* server, PrivateRoomInitData data)
	: MultiRoom(server, data)
{
	max_player_count_ = 5;
}

std::span<Player> FivePlayerRoom::GetRoomPlayers()
{
	return room_players_;
}
