#pragma once
#include <array>
#include "MultiRoom.h"

class TwoPlayerRoom : public MultiRoom
{
	std::array<RoomSession, 2> room_users_;

public:
	TwoPlayerRoom(IOCPServer* server, OpenRoomInitData data);
	TwoPlayerRoom(IOCPServer* server, LockRoomInitData data);

private:
	virtual std::span<RoomSession> GetRoomUsers() override;
};
