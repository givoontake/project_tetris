//#include <iostream>
//#include "PacketHandler.h"
//#include "define.h"
//#include "packetType.h"
//#include "IOCPServer.h"
//
////PacketHandler::PacketHandler(IOCPServer* server) : server_(server)
////{
////}
////
////// 패킷 핸들러를 따로 만들경우 IOCPServer 맴버 변수 접근을 위한 getter가 많이 필요하다..
////// IServer 가상함수로 만들고 업캐스팅을 하는 작업은..불필요하게 복잡해지는 느낌이 있다.
////// IOCP의 맴버 함수로 만들면 편하긴 한데.. switch로 만들꺼라 너무 길어길 것 같아 걱정이다.. 어떻게 해야할까?
////
//////char temp_id[MAX_PLAYER_ID_SIZE] = "master";
//////char temp_password[MAX_PLAYER_PASSWORD_SIZE] = "1234";
//////char temp_name[MAX_PLAYER_NAME_SIZE] = "master";
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
#include "room_lifecycle_tasks.h"

PacketHandler::PacketHandler(IOCPServer& server) : server_(server)
{
}

void PacketHandler::HandleLoginPacket(char* packet, Session* session)
{
	C2S_LOGIN_PACKET* recv_p = reinterpret_cast<C2S_LOGIN_PACKET*>(packet);
	if (!session) return;
	if (session->GetModeState() != ModeState::LOGIN) return;

	std::string login_id = server_.CharBufToString(recv_p->login_id, sizeof(recv_p->login_id));
	std::string password = server_.CharBufToString(recv_p->login_password, sizeof(recv_p->login_password));
	SessionKey session_key = session->GetSessionKey();
	server_.EnqueueDBTask(std::make_unique<DBLoginTask>(session_key, login_id, password));
}

void PacketHandler::HandleMessagePacket(char* packet, Session* session)
{
	if (!session) return;
	C2S_MESSAGE_PACKET* recv_p = reinterpret_cast<C2S_MESSAGE_PACKET*>(packet);
	int msg_size = recv_p->header.size - sizeof(C2S_MESSAGE_PACKET);
	if (msg_size == 0) return;
	int send_p_size = sizeof(S2C_MESSAGE_PACKET) + msg_size;

	if (session->GetModeState() != ModeState::LOBBY) return;
	std::string nickname = session->GetDBInfo().nickname;
	int player_id = session->GetDBInfo().player_id;

	char* send_p = new char[send_p_size];
	S2C_MESSAGE_PACKET front_p;
	front_p.header.size = static_cast<std::uint16_t>(send_p_size);
	front_p.header.type = S2C_MESSAGE;
	front_p.player_id = player_id;
	server_.StringToCharBuf(nickname, front_p.nickname, sizeof(front_p.nickname));
	memcpy(send_p, &front_p, sizeof(S2C_MESSAGE_PACKET));
	memcpy(send_p + sizeof(S2C_MESSAGE_PACKET), reinterpret_cast<char*>(recv_p) + sizeof(C2S_MESSAGE_PACKET), msg_size);

	server_.BroadcastToLobby(send_p);

	delete[] send_p;
}

void PacketHandler::HandleTestPacket(char* packet, Session* session)
{
	if (!session) return;
	C2S_TEST_PACKET* recv_p = reinterpret_cast<C2S_TEST_PACKET*>(packet);
	char* send_p = new char[recv_p->header.size];
	int msg_size = recv_p->header.size - sizeof(C2S_TEST_PACKET);
	S2C_TEST_PACKET front_p;
	front_p.header.size = recv_p->header.size;
	front_p.header.type = S2C_TEST;
	front_p.player_id = session->GetDBInfo().player_id;
	front_p.last_time = recv_p->last_time;
	memcpy(send_p, &front_p, sizeof(S2C_TEST_PACKET));
	memcpy(send_p + sizeof(S2C_TEST_PACKET), reinterpret_cast<char*>(recv_p) + sizeof(C2S_TEST_PACKET), msg_size);

	server_.BroadcastToLobby(send_p);

	delete[] send_p;
}

void PacketHandler::HandleDisconnectPacket(Session* session)
{
	if (!session) return;
	server_.RequestDisconnect(session->GetSessionKey());
}

void PacketHandler::HandleJoinPublicRoomPacket(char* packet, Session* session)
{
	if (!session) return;
	C2S_JOIN_PUBLIC_ROOM_PACKET* join_p = reinterpret_cast<C2S_JOIN_PUBLIC_ROOM_PACKET*>(packet);
	int result = server_.TryJoinRoom(session, join_p->room_gen, "");
	if (result != SUCCESS) server_.SendError(session, result);
}

void PacketHandler::HandleJoinPrivateRoomPacket(char* packet, Session* session)
{
	if (!session) return;
	C2S_JOIN_PRIVATE_ROOM_PACKET* join_p = reinterpret_cast<C2S_JOIN_PRIVATE_ROOM_PACKET*>(packet);
	int result = server_.TryJoinRoom(session, join_p->room_gen, server_.CharBufToString(join_p->room_password, sizeof(join_p->room_password)));
	if (result != SUCCESS) server_.SendError(session, result);
}

void PacketHandler::HandleFastMatchingPacket(char* packet, Session* session)
{
	if (!session) return;
	C2S_FAST_MATCHING_PACKET* matching_p = reinterpret_cast<C2S_FAST_MATCHING_PACKET*>(packet);
	server_.FindMatch(session, matching_p->max_player_count);
}

void PacketHandler::HandleAddFriendRequestPacket(char* packet, Session* session)
{
	if (!session) return;
	C2S_ADD_FRIEND_REQUEST_PACKET* friend_p = reinterpret_cast<C2S_ADD_FRIEND_REQUEST_PACKET*>(packet);

	if (session->GetModeState() != ModeState::LOBBY) return;

	int receiver_id = friend_p->receiver_id;
	SessionKey session_key = session->GetSessionKey();
	DBResultLogin db_info = session->GetDBInfo();
	FriendInfo requester_info{ db_info.player_id, db_info.nickname };
	server_.EnqueueDBTask(std::make_unique<DBAddFriendRequestTask>(session_key, requester_info, receiver_id));
}

void PacketHandler::HandleAcceptFriendPacket(char* packet, Session* session)
{
	if (!session) return;
	C2S_ACCEPT_FRIEND_PACKET* accept_p = reinterpret_cast<C2S_ACCEPT_FRIEND_PACKET*>(packet);
	if (session->GetModeState() != ModeState::LOBBY) return;

	int requester_id = accept_p->requester_id;
	SessionKey session_key = session->GetSessionKey();
	DBResultLogin db_info = session->GetDBInfo();
	FriendInfo acceptor_info{ db_info.player_id, db_info.nickname };
	server_.EnqueueDBTask(std::make_unique<DBAddFriendTask>(session_key, acceptor_info, requester_id));
}

void PacketHandler::HandleDeleteFriendPacket(char* packet, Session* session)
{
	if (!session) return;
	C2S_DELETE_FRIEND_PACKET* delete_p = reinterpret_cast<C2S_DELETE_FRIEND_PACKET*>(packet);
	int target_id = delete_p->target_id;
	if (session->GetModeState() != ModeState::LOBBY) return;
	SessionKey session_key = session->GetSessionKey();
	server_.EnqueueDBTask(std::make_unique<DBDeleteFriendTask>(session_key, target_id));
}

void PacketHandler::HandlePacket(char* packet, Session* session)
{
	if (!session) return;
	switch (reinterpret_cast<PACKET_HEADER*>(packet)->type) {

	case C2S_LOGIN: {
		HandleLoginPacket(packet, session);
		break;
	}

	case C2S_MESSAGE: {
		HandleMessagePacket(packet, session);
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

	case C2S_ADD_PUBLIC_ROOM: {
		const SessionKey session_key = session->GetSessionKey();
		if (!server_.BeginRoomTransition(session_key)) break;
		const int packet_size = reinterpret_cast<PACKET_HEADER*>(packet)->size;
		auto task = std::make_unique<RoomLifecycleTask>(RoomLifecycleTaskType::CREATE_PUBLIC, session_key);
		task->packet.assign(packet, packet + packet_size);
		server_.EnqueueRoomLifecycleTask(std::move(task));
		break;
	}

	case C2S_ADD_PRIVATE_ROOM: {
		const SessionKey session_key = session->GetSessionKey();
		if (!server_.BeginRoomTransition(session_key)) break;
		const int packet_size = reinterpret_cast<PACKET_HEADER*>(packet)->size;
		auto task = std::make_unique<RoomLifecycleTask>(RoomLifecycleTaskType::CREATE_PRIVATE, session_key);
		task->packet.assign(packet, packet + packet_size);
		server_.EnqueueRoomLifecycleTask(std::move(task));
		break;
	}

	case C2S_JOIN_PUBLIC_ROOM: {
		HandleJoinPublicRoomPacket(packet, session);
		break;
	}

	case C2S_JOIN_PRIVATE_ROOM: {
		HandleJoinPrivateRoomPacket(packet, session);
		break;
	}

	case C2S_REQUEST_ROOM_LIST: {
		server_.SendRoomList(session);
		break;
	}

	case C2S_REQUEST_LOBBY_PLAYER_LIST: {
		server_.SendLobbyPlayerList(session);
		break;
	}

	case C2S_REQUEST_FRIEND_LIST: {
		server_.SendFriendList(session);
		break;
	}

	case C2S_REQUEST_RANKINGS: {
		server_.SendRankings(session);
		break;
	}

	case C2S_FAST_MATCHING: {
		HandleFastMatchingPacket(packet, session);
		break;
	}

	case C2S_ADD_FRIEND_REQUEST: {
		HandleAddFriendRequestPacket(packet, session);
		break;
	}

	case C2S_ACCEPT_FRIEND: {
		HandleAcceptFriendPacket(packet, session);
		break;
	}

	case C2S_DELETE_FRIEND: {
		HandleDeleteFriendPacket(packet, session);
		break;
	}
	}
}
