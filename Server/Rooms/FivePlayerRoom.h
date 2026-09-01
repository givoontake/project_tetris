#pragma once
#include <array>
#include "MultiRoom.h"

class FivePlayerRoom : public MultiRoom
{
	std::array<RoomSession, 5> room_users_;

public:
	FivePlayerRoom(IOCPServer* server, OpenRoomInitData data);
	FivePlayerRoom(IOCPServer* server, LockRoomInitData data);

private:
	virtual std::span<RoomSession> GetRoomUsers() override;
};
