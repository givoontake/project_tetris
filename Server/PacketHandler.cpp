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
		front_p.type = S2C_TEST;
		front_p.id = recv_p->id;
		memcpy(send_p, &front_p, sizeof(S2C_MESSAGE_PACKET)); // 구조체 부분 복사
		memcpy(send_p + sizeof(S2C_MESSAGE_PACKET), reinterpret_cast<char*>(recv_p) + sizeof(C2S_MESSAGE_PACKET), msg_size); // 가변데이터 복사

		server->BroadCastLobby(send_p);

		delete[] send_p;
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

		// 잠만 갑자기 생각난건데, 전체 패킷 크기를 size에 넣지 않는다면.. 이거 패킷 잘려오면 앞에 패킷 사이즈(구조체 자체)만 보고 뒤에 메세지는 크기에 들어가지 않아 무시되면서 
		// 문제가 발생할 수 있겠는데??
		// 아 문제는 없네. 근데 굉~장히 귀찮다. 그냥 사이즈에 전체 패킷 크기를 넣어 보내면 메세지 크기는 없어도 될 것 같은데??
		// 그러니까 이 구조체의 size는 구조체 자체 크기 + 가변길이 데이터 크기가 저장되어 있어야 하는거지
		// 그러면 수신측 처리에서 recv_p->size에서 해당 패킷 구조체 길이를 뺀 값이 가변길이 데이터잖아. 이렇게 복잡하게 갈 필요가 없어보인다.

		//int real_size = recv_p->size + recv_p->message_size; // 신뢰 가능한 패킷이면 p->size는 sizeof(S2C_TEST_PACKET)이다. 

		//char* recv_p_msg = new char[real_size];
		//memcpy(recv_p_msg, recv_p, sizeof(S2C_TEST_PACKET)); // 오버런 오류는 p->size가 신뢰되지 못한 값일 가능성이 있다는 경고
		//memcpy(recv_p_msg + sizeof(S2C_TEST_PACKET), reinterpret_cast<char*>(recv_p) + sizeof(S2C_TEST_PACKET), (recv_p->message_size));

		//for (auto& user : users) {
		//	if (!user->GetUse()) continue;
		//	user->SendPacket(recv_p_msg);
		//}

		break;
	}
		
	case C2S_DISCONNECT: { // 로비에서 disconnect될 때 -> 이것도 그냥 서버에서 처리하도록 하자.
		C2S_DISCONNECT_PACKET* recv_p = reinterpret_cast<C2S_DISCONNECT_PACKET*>(&packet);
		server->Disconnect(recv_p->id);
		break;
	}

					   // 방 추가는 하면서 방을 만든 사람을 방에 추가하는건 없네..?
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
