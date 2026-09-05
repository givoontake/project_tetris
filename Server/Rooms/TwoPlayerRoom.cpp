#include "TwoPlayerRoom.h"

TwoPlayerRoom::TwoPlayerRoom(TetrisServer* server, PublicRoomInitData data)
	: MultiRoom(server, data)
{
	max_player_count_ = 2;
}

TwoPlayerRoom::TwoPlayerRoom(TetrisServer* server, PrivateRoomInitData data)
	: MultiRoom(server, data)
{
	max_player_count_ = 2;
}

std::span<Player> TwoPlayerRoom::GetRoomPlayers()
{
	return room_players_;
}
