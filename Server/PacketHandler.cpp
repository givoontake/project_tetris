#include "PacketHandler.h"
//#include "define.h"
#include "Session.h"

PacketHandler::PacketHandler(IServer* i_server) : server_interface(i_server)
{

}

void PacketHandler::ProcessPacket(char* packet)
{
	std::array<std::unique_ptr<Session>, MAX_USER>& users = server_interface->GetSessionList();

	switch (packet[2]) {
	case C2S_LOGIN:
		// 받아서 따로 서버에서 변경되는 패킷 내용이 없다.
		for (auto& user : users) {
			if (!user->GetUse()) continue;
			S2C_LOGIN_PACKET* p = reinterpret_cast<S2C_LOGIN_PACKET*>(packet);
			p->type = S2C_LOGIN;
			user->SendPacket(reinterpret_cast<char*>(p));
		}
		break;
		
	case C2S_MESSAGE:
		// 받아서 따로 서버에서 변경되는 패킷 내용이 없다.
		for (auto& user : users) {
			if (!user->GetUse()) continue;
			S2C_MESSAGE_PACKET* p = reinterpret_cast<S2C_MESSAGE_PACKET*>(packet);
			p->type = S2C_MESSAGE;
			user->SendPacket(reinterpret_cast<char*>(p));
		}
		break;

	case C2S_TEST:
		// 받아서 따로 서버에서 변경되는 패킷 내용이 없다.
		for (auto& user : users) {
			if (!user->GetUse()) continue;
			S2C_TEST_PACKET* p = reinterpret_cast<S2C_TEST_PACKET*>(packet);
			p->type = S2C_TEST;
			int real_size = p->size + p->message_size; // 신뢰 가능한 패킷이면 p->size는 sizeof(S2C_TEST_PACKET)이다. 
			char* pp = new char[real_size];
			memcpy(pp, p, sizeof(S2C_TEST_PACKET));
			memcpy(pp + sizeof(S2C_TEST_PACKET), reinterpret_cast<char*>(p)+ sizeof(S2C_TEST_PACKET), (p->message_size));
			user->SendPacket(pp);
			delete[] pp;
		}
		break;
	}

}
