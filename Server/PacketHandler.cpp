#include "PacketHandler.h"
#include "define.h"
#include "packetType.h"
#include "IOCPServer.h"

PacketHandler::PacketHandler(IOCPServer* server) : server(server)
{
}

// 패킷 핸들러를 따로 만들경우 IOCPServer 맴버 변수 접근을 위한 getter가 많이 필요하다..
// IServer 가상함수로 만들고 업캐스팅을 하는 작업은..불필요하게 복잡해지는 느낌이 있다.
// IOCP의 맴버 함수로 만들면 편하긴 한데.. switch로 만들꺼라 너무 길어길 것 같아 걱정이다.. 어떻게 해야할까?

void PacketHandler::HandlePacket(char* packet)
{
	//static auto& users = server->GetSessionList();
	//static auto& rooms = server->GetRoomList();
	switch (packet[2]) {
	case C2S_LOGIN: {
		C2S_LOGIN_PACKET* recv_p = reinterpret_cast<C2S_LOGIN_PACKET*>(packet);
		S2C_LOGIN_PACKET send_p;
		send_p.size = sizeof(S2C_LOGIN_PACKET);
		send_p.type = S2C_LOGIN;
		send_p.id = recv_p->id;
		server->SendToSelf((char*)&send_p, send_p.id);
		break;
	}
		
	case C2S_MESSAGE: { // 테스트는 메세지 가변으로 해놓고 이건 가변으로 안했네;; 뭐했냐 나
		C2S_MESSAGE_PACKET* recv_p = reinterpret_cast<C2S_MESSAGE_PACKET*>(packet);
		char* send_p = new char[recv_p->size];
		int msg_size = recv_p->size - sizeof(C2S_MESSAGE_PACKET);
		S2C_MESSAGE_PACKET front_p;
		front_p.size = recv_p->size;
		front_p.type = S2C_MESSAGE;
		front_p.id = recv_p->id;
		memcpy(send_p, &front_p, sizeof(S2C_MESSAGE_PACKET)); // 구조체 부분 복사
		memcpy(send_p + sizeof(S2C_MESSAGE_PACKET), reinterpret_cast<char*>(recv_p) + sizeof(C2S_MESSAGE_PACKET), msg_size); // 가변데이터 복사

		server->BroadCastLobby(send_p);

		delete[] send_p;

		break;
	}

	case C2S_TEST: {
		// recv_p->size에 구조체 + 가변길이 데이터가 들어있다는 가정하에 구현->나중에 테스트 프로그램 로직도 바꿔야함
		C2S_TEST_PACKET* recv_p = reinterpret_cast<C2S_TEST_PACKET*>(packet);
		char* send_p = new char[recv_p->size];
		int msg_size = recv_p->size - sizeof(C2S_TEST_PACKET);
		S2C_TEST_PACKET front_p;
		front_p.size = recv_p->size;
		front_p.type = S2C_TEST;
		front_p.id = recv_p->id;
		front_p.last_time = recv_p->last_time;
		memcpy(send_p, &front_p, sizeof(S2C_TEST_PACKET)); // 구조체 부분 복사
		memcpy(send_p + sizeof(S2C_TEST_PACKET), reinterpret_cast<char*>(recv_p) + sizeof(C2S_TEST_PACKET), msg_size); // 가변데이터 복사

		server->BroadCastLobby(send_p);

		delete[] send_p;

		break;
	}
		
	case C2S_DISCONNECT: { 
		C2S_DISCONNECT_PACKET* recv_p = reinterpret_cast<C2S_DISCONNECT_PACKET*>(packet);
		server->Disconnect(recv_p->id);
		break;
	}

	case C2S_ADD_OPEN_ROOM: {
		server->CreateRoom(packet);
		break;
	}

	case C2S_ADD_LOCK_ROOM: {
		server->CreateRoom(packet);
		break;
	}
	}
	
}
