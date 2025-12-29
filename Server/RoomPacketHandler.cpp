#include "RoomPacketHandler.h"
#include "TetrisRoom.h"
#include "IOCPServer.h"

RoomPacketHandler::RoomPacketHandler(TetrisRoom* room, IOCPServer* server) : room(room), PacketHandler(server)
{

}

void RoomPacketHandler::HandlePacket(char* packet, int user_index)
{
	PrintPacketType(packet[2]);

	switch (packet[2]) {
	case C2S_ADD_USER: {
		//C2S_ADD_USER_PACKET* recv_p = reinterpret_cast<C2S_ADD_USER_PACKET*>(packet);
		room->AddUser(server->GetSession(user_index));
		break;
	}

	case C2S_DELETE_USER: {
		room->DeleteUser(server->GetSession(user_index)->GetId());
		break;
	}

	case C2S_READY: {
		//C2S_READY_PACKET* recv_p = reinterpret_cast<C2S_READY_PACKET*>(packet);
		room->ReadyUser(server->GetSession(user_index)->GetId());
		break;
	}

	case C2S_KICK: {
		C2S_KICK_PACKET* recv_p = reinterpret_cast<C2S_KICK_PACKET*>(packet);
		room->KickUser(server->GetSession(user_index)->GetId(), recv_p->kick_user_id);
		break;
	}

	case C2S_START: {
		//C2S_START_PACKET* recv_p = reinterpret_cast<C2S_START_PACKET*>(packet);
		room->StartGame(server->GetSession(user_index)->GetId());
		break;
	}

	case C2S_MOVE: {
		C2S_MOVE_PACKET* recv_p = reinterpret_cast<C2S_MOVE_PACKET*>(packet);
		TaskInfo new_task;
		new_task.id = server->GetSession(user_index)->GetId();
		new_task.type = static_cast<EVENT_TYPE>(recv_p->move_type);
		room->GetTasks().AddTask(new_task);
		break;
	}
	}
}

