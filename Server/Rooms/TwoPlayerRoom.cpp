#include "TwoPlayerRoom.h"

TwoPlayerRoom::TwoPlayerRoom(IOCPServer* server, OpenRoomInitData data)
	: MultiRoom(server, data)
{
	max_user = 2;
}

TwoPlayerRoom::TwoPlayerRoom(IOCPServer* server, LockRoomInitData data)
	: MultiRoom(server, data)
{
	max_user = 2;
}

std::span<RoomSession> TwoPlayerRoom::GetRoomUsers()
{
	return room_users;
}
