#include "PacketHandler.h"
//#include "define.h"
#include "Session.h"

PacketHandler::PacketHandler(IServer* i_server) : server_interface(i_server)
{

}

void PacketHandler::HandlePacket(char* packet)
{
	std::array<std::unique_ptr<Session>, MAX_USER>& users = server_interface->GetSessionList();

	// p->size 같은 경우 사실 서버-클라간 패킷 구성이 같다면 굳이 필요없지만, 세션의 DISCONNECT 로직 때문에 추가하다 그냥 다 추가하기로 했다.
	switch (packet[2]) {
	case C2S_LOGIN: {
		S2C_LOGIN_PACKET* p = reinterpret_cast<S2C_LOGIN_PACKET*>(packet);
		p->size = sizeof(S2C_LOGIN_PACKET);
		p->type = S2C_LOGIN;

		for (auto& user : users) {
			if (!user->GetUse()) continue;
			user->SendPacket(reinterpret_cast<char*>(p));
		}
		break;
	}
		
	case C2S_MESSAGE: {
		S2C_MESSAGE_PACKET* p = reinterpret_cast<S2C_MESSAGE_PACKET*>(packet);
		p->size = sizeof(S2C_MESSAGE_PACKET);
		p->type = S2C_MESSAGE;

		for (auto& user : users) {
			if (!user->GetUse()) continue;
			user->SendPacket(reinterpret_cast<char*>(p));
		}
		break;
	}

	case C2S_TEST: {
		S2C_TEST_PACKET* p = reinterpret_cast<S2C_TEST_PACKET*>(packet);
		p->type = S2C_TEST;
		int real_size = p->size + p->message_size; // 신뢰 가능한 패킷이면 p->size는 sizeof(S2C_TEST_PACKET)이다. 
		char* pp = new char[real_size];
		memcpy(pp, p, sizeof(S2C_TEST_PACKET)); // 오버런 오류는 p->size가 신뢰되지 못한 값일 가능성이 있다는 경고
		memcpy(pp + sizeof(S2C_TEST_PACKET), reinterpret_cast<char*>(p) + sizeof(S2C_TEST_PACKET), (p->message_size));

		for (auto& user : users) {
			if (!user->GetUse()) continue;
			user->SendPacket(pp);
		}

		delete[] pp;
		break;
	}
		
	case C2S_DISCONNECT: {

		S2C_DISCONNECT_PACKET* p = reinterpret_cast<S2C_DISCONNECT_PACKET*>(packet);
		p->size = sizeof(S2C_DISCONNECT_PACKET);
		p->type = S2C_DISCONNECT;
		server_interface->Disconnect(p->id);
	}
	}
	
}

void PacketHandler::Disconnect(int user_id)
{
	server_interface->Disconnect(user_id);
}
