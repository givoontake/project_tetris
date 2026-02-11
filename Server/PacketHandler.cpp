#include <iostream>
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

//char temp_id[MAX_USER_ID] = "master";
//char temp_password[MAX_USER_PASSWORD] = "1234";
//char temp_name[MAX_USER_NAME] = "master";

void PacketHandler::HandlePacket(char* packet, int user_index)
{
	//static auto& users = server->GetSessionList();
	//static auto& rooms = server->GetRoomList();
	PrintPacketType(packet[2]);
	//short* p_size;
	//p_size = reinterpret_cast<short*>(packet);
	//std::cout << "packet_size: " << *p_size << std::endl;

	switch (packet[2]) {

	case C2S_LOGIN: {
		C2S_LOGIN_PACKET* recv_p = reinterpret_cast<C2S_LOGIN_PACKET*>(packet);
		int id = server->GetSession(user_index)->GetId();
		int index = server->GetSession(user_index)->GetIndex();
		// null은 있을수도, 없을수도 있음. 그래서 일단 전체를 받아야함. strnlen(buf, max_size) -> null 직전까지 길이 반환, 안만나면 최대길이 반환
		std::string login_id = server->CharBufToString(recv_p->login_id, sizeof(recv_p->login_id));
		std::string password = server->CharBufToString(recv_p->login_password, sizeof(recv_p->login_password));
		Database& db = server->GetDB();
		auto task_login = [&db, id, index, login_id, password]() {
			db.ExecuteLogin(id, index, login_id, password);
			};
		
		db.Enqueue(task_login);
		//server->GetDB().Enqueue
		//// 우선은 연결 요청이 들어오는 즉시 세션을 사용하도록 함. -> 나중에 반드시 바꿔야함
		//S2C_LOGIN_PACKET send_p;
		//send_p.size = sizeof(S2C_LOGIN_PACKET);
		//send_p.type = S2C_LOGIN;
		//memcpy(send_p.user_name, temp_name, MAX_USER_NAME);
		//if (!memcmp(temp_id, recv_p->user_id, MAX_USER_ID) && !memcmp(temp_password, recv_p->user_password, MAX_USER_PASSWORD)) send_p.id = server->GetSession(user_index)->GetId();	
		//else send_p.id = -1;

		//std::cout << "size: " << recv_p->size << ", type: " << (int)recv_p->type << ", id: " << recv_p->user_id << ", pw: " << recv_p->user_password << std::endl;

		//server->SendToSelf((char*)&send_p, user_index);

		break;
	}
		
	case C2S_MESSAGE: { // 테스트는 메세지 가변으로 해놓고 이건 가변으로 안했네;; 뭐했냐 나
		C2S_MESSAGE_PACKET* recv_p = reinterpret_cast<C2S_MESSAGE_PACKET*>(packet);
		int msg_size = recv_p->size - sizeof(C2S_MESSAGE_PACKET);
		if (msg_size == 0) return;
		int send_p_size = sizeof(S2C_MESSAGE_PACKET) + msg_size;
		char* send_p = new char[send_p_size];
		S2C_MESSAGE_PACKET front_p;
		front_p.size = send_p_size;
		front_p.type = S2C_MESSAGE;
		front_p.id = recv_p->id;
		server->StringToCharBuf(server->GetSession(user_index)->GetInfo().nickname, front_p.user_name, sizeof(front_p.user_name));
		memcpy(send_p, &front_p, sizeof(S2C_MESSAGE_PACKET)); // 구조체 부분 복사
		memcpy(send_p + sizeof(S2C_MESSAGE_PACKET), reinterpret_cast<char*>(recv_p) + sizeof(C2S_MESSAGE_PACKET), msg_size); // 가변데이터 복사
		
		server->BroadCastLobby(send_p);

		delete[] send_p;

		break;
	}

	case C2S_TEST: {
		// recv_p->size에 구조체 + 가변길이 데이터가 들어있다는 가정하에 구현->나중에 테스트 프로그램 로직도 바꿔야함
		//std::cout << "테스트 패킷 수신" << std::endl;
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
		server->Disconnect(user_index);
		break;
	}

	case C2S_ADD_OPEN_ROOM: {
		server->CreateRoom(packet, user_index);
		break;
	}

	case C2S_ADD_LOCK_ROOM: {
		server->CreateRoom(packet, user_index);
		break;
	}
	}
	
}

