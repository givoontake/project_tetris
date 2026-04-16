//#include <iostream>
//#include "PacketHandler.h"
//#include "define.h"
//#include "packetType.h"
//#include "IOCPServer.h"
//
////PacketHandler::PacketHandler(IOCPServer* server) : server(server)
////{
////}
////
////// 패킷 핸들러를 따로 만들경우 IOCPServer 맴버 변수 접근을 위한 getter가 많이 필요하다..
////// IServer 가상함수로 만들고 업캐스팅을 하는 작업은..불필요하게 복잡해지는 느낌이 있다.
////// IOCP의 맴버 함수로 만들면 편하긴 한데.. switch로 만들꺼라 너무 길어길 것 같아 걱정이다.. 어떻게 해야할까?
////
//////char temp_id[MAX_USER_ID] = "master";
//////char temp_password[MAX_USER_PASSWORD] = "1234";
//////char temp_name[MAX_USER_NAME] = "master";
////
////void PacketHandler::HandlePacket(char* packet, Session* request_session)
////{
////
////	
////}
//
#include "PacketHandler.h"
#include "define_packets.h"
#include "packet_types.h"
#include "IOCPServer.h"

PacketHandler::PacketHandler(IOCPServer& server) : server(server)
{
}

void PacketHandler::HandleLoginPacket(char* packet, Session& session, int request_gen)
{
	server.HandleLoginPacket(packet, session, request_gen);
}

void PacketHandler::HandleMessagePacket(char* packet, Session& session, int request_gen)
{
	server.HandleMessagePacket(packet, session, request_gen);
}

void PacketHandler::HandleTestPacket(char* packet, Session& session)
{
	server.HandleTestPacket(packet, session);
}

void PacketHandler::HandleDisconnectPacket(Session& session)
{
	server.HandleDisconnectPacket(session);
}

void PacketHandler::HandleJoinOpenRoomPacket(char* packet, Session& session, int request_gen)
{
	server.HandleJoinOpenRoomPacket(packet, session, request_gen);
}

void PacketHandler::HandleJoinLockRoomPacket(char* packet, Session& session, int request_gen)
{
	server.HandleJoinLockRoomPacket(packet, session, request_gen);
}

void PacketHandler::HandleFastMatchingPacket(char* packet, Session& session, int request_gen)
{
	server.HandleFastMatchingPacket(packet, session, request_gen);
}

void PacketHandler::HandleRequestFriendPacket(char* packet, Session& session, int request_gen)
{
	server.HandleRequestFriendPacket(packet, session, request_gen);
}

void PacketHandler::HandleAcceptFriendPacket(char* packet, Session& session, int request_gen)
{
	server.HandleAcceptFriendPacket(packet, session, request_gen);
}

void PacketHandler::HandleDeleteFriendPacket(char* packet, Session& session, int request_gen)
{
	server.HandleDeleteFriendPacket(packet, session, request_gen);
}

void PacketHandler::HandlePacket(char* packet, Session& session, int request_gen)
{
	switch (reinterpret_cast<PacketHeader*>(packet)->type) {

	case C2S_LOGIN: {
		HandleLoginPacket(packet, session, request_gen);
		break;
	}

	case C2S_MESSAGE: {
		HandleMessagePacket(packet, session, request_gen);
		break;
	}

	case C2S_TEST: {
		HandleTestPacket(packet, session);
		break;
	}

	case C2S_DISCONNECT: {
		HandleDisconnectPacket(session);
		break;
	}

	case C2S_ADD_OPEN_ROOM: {
		server.CreateOpenRoom(packet, session, request_gen);
		break;
	}

	case C2S_ADD_LOCK_ROOM: {
		server.CreateLockRoom(packet, session, request_gen);
		break;
	}

	case C2S_JOIN_OPEN_ROOM: {
		HandleJoinOpenRoomPacket(packet, session, request_gen);
		break;
	}

	case C2S_JOIN_LOCK_ROOM: {
		HandleJoinLockRoomPacket(packet, session, request_gen);
		break;
	}

	case C2S_REQUEST_ROOM_LIST: {
		server.SendRoomList(session, request_gen);
		break;
	}

	case C2S_REQUEST_LOBBY_USER_LIST: {
		server.SendLobbyUserList(session, request_gen);
		break;
	}

	case C2S_REQUEST_FRIEND_LIST: {
		server.SendFriendList(session, request_gen);
		break;
	}

	case C2S_REQUEST_RANKING: {
		server.SendRanking(session, request_gen);
		break;
	}

	case C2S_FAST_MATCHING: {
		HandleFastMatchingPacket(packet, session, request_gen);
		break;
	}

	case C2S_REQUEST_FRIEND: {
		HandleRequestFriendPacket(packet, session, request_gen);
		break;
	}

	case C2S_ACCEPT_FRIEND: {
		HandleAcceptFriendPacket(packet, session, request_gen);
		break;
	}

	case C2S_DELETE_FRIEND: {
		HandleDeleteFriendPacket(packet, session, request_gen);
		break;
	}
	}
}
