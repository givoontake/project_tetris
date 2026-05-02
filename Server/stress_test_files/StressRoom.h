#pragma once
#include "../MultiRoom.h"

class StressRoom : public MultiRoom
{
public:
	StressRoom(IOCPServer* server, OpenRoomInitData data);
	StressRoom(IOCPServer* server, LockRoomInitData data);
	~StressRoom();

	virtual void ProcessPlayTasks() override;
	virtual void StartStressGame() override;
};
