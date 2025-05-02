#include<iostream>
#include "PacketHandler.h"
//#include "define.h"
#include "Session.h"

PacketHandler::PacketHandler()
{
}

void PacketHandler::ProcessPacket(char* packet)
{
	std::array<Session, MAX_USER>& users = manager_interface->GetSessionList();

	switch (packet[1]) {
	case S2C_TEST: {
		int now_time = manager_interface->GetCurrentTimeMS();
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
