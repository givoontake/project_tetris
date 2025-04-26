#pragma once
#include "Interface.h"

class PacketHandler : IPacketHandler
{
public:
	PacketHandler();
	IServer* server_interface;
	virtual void ProcessPacket(char* pakcet);
};

// 참조 맴버는 생성자 초기화가 강제되어서, 확실히 점점 서로 코드적으로 의존성이 강해지는 느낌이 든다. 그냥 포인터로 냅두면 그런 것을 고려할 필요는 없는데.. 일단 포인터로 해보겠다.
