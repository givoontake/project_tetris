#include "FivePlayerRoom.h"

FivePlayerRoom::FivePlayerRoom(TetrisServer* server, PublicRoomInitData data)
	: MultiRoom(server, data)
{
	max_player_count_ = 5;
}

FivePlayerRoom::FivePlayerRoom(TetrisServer* server, PrivateRoomInitData data)
	: MultiRoom(server, data)
{
	max_player_count_ = 5;
}

std::span<Player> FivePlayerRoom::GetRoomPlayers()
{
	return room_players_;
}
