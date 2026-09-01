#include "FivePlayerRoom.h"

FivePlayerRoom::FivePlayerRoom(IOCPServer* server, OpenRoomInitData data)
	: MultiRoom(server, data)
{
	max_user = 5;
}

FivePlayerRoom::FivePlayerRoom(IOCPServer* server, LockRoomInitData data)
	: MultiRoom(server, data)
{
	max_user = 5;
}

std::span<RoomSession> FivePlayerRoom::GetRoomUsers()
{
	return room_users;
}
