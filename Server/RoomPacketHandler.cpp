#include "RoomPacketHandler.h"
#include "TetrisRoom.h"
#include "IOCPServer.h"

RoomPacketHandler::RoomPacketHandler(TetrisRoom* room, IOCPServer* server) : room(room), PacketHandler(server)
{

}

void RoomPacketHandler::HandlePacket(char* packet, int user_index)
{
	switch (packet[2]) {
	case C2S_ADD_USER: {
		C2S_ADD_USER_PACKET* recv_p = reinterpret_cast<C2S_ADD_USER_PACKET*>(packet);
		room->AddUser(server->GetSession(recv_p->id));
		break;
	}
	}
}

