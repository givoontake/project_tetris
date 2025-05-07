#include<iostream>
#include "PacketHandler.h"
//#include "define.h"
#include "Session.h"

PacketHandler::PacketHandler(ITestManager* i_manager) : manager_interface(i_manager)
{

}

void PacketHandler::ProcessPacket(char* packet)
{
	std::array<std::unique_ptr<Session>, MAX_USER>& users = manager_interface->GetSessionList();
	S2C_TEST_PACKET* p = reinterpret_cast<S2C_TEST_PACKET*>(packet);
	if (p->id < 0 || p->id >= MAX_ARRAY_SIZE) return;
	if (!users[p->id]->GetUse()) return;

	switch (packet[2]) {
	case S2C_TEST: {
		long long now_time = manager_interface->GetCurrentTimeMS();
		// 받아서 따로 서버에서 변경되는 패킷 내용이 없다.
		S2C_TEST_PACKET* p = reinterpret_cast<S2C_TEST_PACKET*>(packet);
		manager_interface->AdjustClientNumber(now_time, p);
		break;
	}
	default: {
		std::cout << "잘못된 패킷, 코드 수정이 필요합니다. \n";
		break;
	}
				
	}
}
