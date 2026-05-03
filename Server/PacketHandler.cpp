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
#include "ViewSession.h"

PacketHandler::PacketHandler(IOCPServer& server) : server(server)
{
}

void PacketHandler::HandleLoginPacket(char* packet, const SP<Session>& session)
{
	C2S_LOGIN_PACKET* recv_p = reinterpret_cast<C2S_LOGIN_PACKET*>(packet);
	if (!session) return;
	if (session->GetModeState() != MODE_STATE::LOGIN) return;

	std::string login_id = server.CharBufToString(recv_p->login_id, sizeof(recv_p->login_id));
	std::string password = server.CharBufToString(recv_p->login_password, sizeof(recv_p->login_password));
	if (login_id == VIEW_SESSION_LOGIN_ID && password == VIEW_SESSION_PASSWORD) {
		server.GetViewSession().InitSession(session);
		return;
	}

	if (server.IsStressTestMode()) {
		server.LoginStressTestSession(session, login_id);
		return;
	}

	Database& repr_db = server.GetDB();
	SessionKey key = session->GetSessionKey();
	auto task_login = [&repr_db, key, login_id, password]() {
		repr_db.ExecuteLogin(key, login_id, password);
		};

	repr_db.Enqueue(task_login, session);
}

void PacketHandler::HandleTestLoginPacket(char* packet, const SP<Session>& session)
{
	if (!session) return;
	C2S_TEST_LOGIN_PACKET* recv_p = reinterpret_cast<C2S_TEST_LOGIN_PACKET*>(packet);
	server.LoginStressTestSession(session, "tester" + std::to_string(recv_p->temp_id + 1), recv_p->client_time);
}

void PacketHandler::HandleStressEnterMatchPacket(const SP<Session>& session)
{
	if (!session) return;
	int result = server.EnterStressRoom(session);
	if (result != SUCCESS) server.SendError(session, result);
}

void PacketHandler::HandleMessagePacket(char* packet, const SP<Session>& session)
{
	if (!session) return;
	C2S_MESSAGE_PACKET* recv_p = reinterpret_cast<C2S_MESSAGE_PACKET*>(packet);
	int msg_size = recv_p->header.size - sizeof(C2S_MESSAGE_PACKET);
	if (msg_size == 0) return;
	int send_p_size = sizeof(S2C_MESSAGE_PACKET) + msg_size;

	if (session->GetModeState() != MODE_STATE::LOBBY) return;
	std::string nickname = session->GetDBInfo().nickname;
	int id = session->GetDBInfo().id;

	char* send_p = new char[send_p_size];
	S2C_MESSAGE_PACKET front_p;
	front_p.header.size = static_cast<std::uint16_t>(send_p_size);
	front_p.header.type = S2C_MESSAGE;
	front_p.id = id;
	server.StringToCharBuf(nickname, front_p.user_name, sizeof(front_p.user_name));
	memcpy(send_p, &front_p, sizeof(S2C_MESSAGE_PACKET));
	memcpy(send_p + sizeof(S2C_MESSAGE_PACKET), reinterpret_cast<char*>(recv_p) + sizeof(C2S_MESSAGE_PACKET), msg_size);

	server.BroadCastToLobby(send_p);

	delete[] send_p;
}

void PacketHandler::HandleTestPacket(char* packet, const SP<Session>& session)
{
	if (!session) return;
	C2S_TEST_PACKET* recv_p = reinterpret_cast<C2S_TEST_PACKET*>(packet);
	char* send_p = new char[recv_p->header.size];
	int msg_size = recv_p->header.size - sizeof(C2S_TEST_PACKET);
	S2C_TEST_PACKET front_p;
	front_p.header.size = recv_p->header.size;
	front_p.header.type = S2C_TEST;
	front_p.id = session->GetDBInfo().id;
	front_p.last_time = recv_p->last_time;
	memcpy(send_p, &front_p, sizeof(S2C_TEST_PACKET));
	memcpy(send_p + sizeof(S2C_TEST_PACKET), reinterpret_cast<char*>(recv_p) + sizeof(C2S_TEST_PACKET), msg_size);

	server.BroadCastToLobby(send_p);

	delete[] send_p;
}

void PacketHandler::HandleDisconnectPacket(const SP<Session>& session)
{
	if (!session) return;
	if (session->BeginDeactivate()) server.BeginDisconnect(session);
	if (session->TryDeactivate()) server.TryDisconnect(session);
}

void PacketHandler::HandleJoinOpenRoomPacket(char* packet, const SP<Session>& session)
{
	if (!session) return;
	C2S_JOIN_OPEN_ROOM_PACKET* join_p = reinterpret_cast<C2S_JOIN_OPEN_ROOM_PACKET*>(packet);
	int result = server.TryJoinRoom(session, join_p->room_gen, "");
	if (result != SUCCESS) server.SendError(session, result);
}

void PacketHandler::HandleJoinLockRoomPacket(char* packet, const SP<Session>& session)
{
	if (!session) return;
	C2S_JOIN_LOCK_ROOM_PACKET* join_p = reinterpret_cast<C2S_JOIN_LOCK_ROOM_PACKET*>(packet);
	int result = server.TryJoinRoom(session, join_p->room_gen, server.CharBufToString(join_p->room_password, sizeof(join_p->room_password)));
	if (result != SUCCESS) server.SendError(session, result);
}

void PacketHandler::HandleFastMatchingPacket(char* packet, const SP<Session>& session)
{
	if (!session) return;
	C2S_FAST_MATCHING_PACKET* matching_p = reinterpret_cast<C2S_FAST_MATCHING_PACKET*>(packet);
	server.FindMatch(session, matching_p->max_user);
}

void PacketHandler::HandleRequestFriendPacket(char* packet, const SP<Session>& session)
{
	if (!session) return;
	if (server.IsStressTestMode()) {
		server.SendError(session, ERROR_CODE::INVALID_REQUEST);
		return;
	}
	C2S_REQUEST_FRIEND_PACKET* friend_p = reinterpret_cast<C2S_REQUEST_FRIEND_PACKET*>(packet);

	if (session->GetModeState() != MODE_STATE::LOBBY) return;

	int recver_id = friend_p->recver_id;
	Database& repr_db = server.GetDB();
	SessionKey key = session->GetSessionKey();
	DBResultLogin db_info = session->GetDBInfo();
	FriendInfo requester_info{ db_info.id, db_info.nickname };
	auto task_afr = [&repr_db, key, requester_info, recver_id]() {
		repr_db.ExecuteAddFriendRequest(key, requester_info, recver_id);
		};

	repr_db.Enqueue(task_afr, session);
}

void PacketHandler::HandleAcceptFriendPacket(char* packet, const SP<Session>& session)
{
	if (!session) return;
	if (server.IsStressTestMode()) {
		server.SendError(session, ERROR_CODE::INVALID_REQUEST);
		return;
	}
	C2S_ACCEPT_FRIEND_PACKET* accept_p = reinterpret_cast<C2S_ACCEPT_FRIEND_PACKET*>(packet);
	if (session->GetModeState() != MODE_STATE::LOBBY) return;

	int requester_id = accept_p->requester_id;
	Database& repr_db = server.GetDB();
	SessionKey key = session->GetSessionKey();
	DBResultLogin db_info = session->GetDBInfo();
	FriendInfo accepter_info{ db_info.id, db_info.nickname };
	auto task_af = [&repr_db, key, accepter_info, requester_id]() {
		repr_db.ExecuteAddFriend(key, accepter_info, requester_id);
		};

	repr_db.Enqueue(task_af, session);
}

void PacketHandler::HandleDeleteFriendPacket(char* packet, const SP<Session>& session)
{
	if (!session) return;
	if (server.IsStressTestMode()) {
		server.SendError(session, ERROR_CODE::INVALID_REQUEST);
		return;
	}
	C2S_DELETE_FRIEND_PACKET* delete_p = reinterpret_cast<C2S_DELETE_FRIEND_PACKET*>(packet);
	int target_id = delete_p->target_id;
	if (session->GetModeState() != MODE_STATE::LOBBY) return;
	Database& repr_db = server.GetDB();
	SessionKey key = session->GetSessionKey();
	auto task_df = [&repr_db, key, target_id]() {
		repr_db.ExecuteDeleteFriend(key, target_id);
		};

	repr_db.Enqueue(task_df, session);
}

void PacketHandler::HandlePacket(char* packet, const SP<Session>& session)
{
	if (!session) return;
	switch (reinterpret_cast<PacketHeader*>(packet)->type) {

	case C2S_LOGIN: {
		HandleLoginPacket(packet, session);
		break;
	}

	case C2S_TEST_LOGIN: {
		HandleTestLoginPacket(packet, session);
		break;
	}

	case C2S_STRESS_ENTER_MATCH: {
		HandleStressEnterMatchPacket(session);
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

	case C2S_ADD_OPEN_ROOM: {
		server.CreateOpenRoom(packet, session);
		break;
	}

	case C2S_ADD_LOCK_ROOM: {
		server.CreateLockRoom(packet, session);
		break;
	}

	case C2S_JOIN_OPEN_ROOM: {
		HandleJoinOpenRoomPacket(packet, session);
		break;
	}

	case C2S_JOIN_LOCK_ROOM: {
		HandleJoinLockRoomPacket(packet, session);
		break;
	}

	case C2S_REQUEST_ROOM_LIST: {
		server.SendRoomList(session);
		break;
	}

	case C2S_REQUEST_LOBBY_USER_LIST: {
		server.SendLobbyUserList(session);
		break;
	}

	case C2S_REQUEST_FRIEND_LIST: {
		server.SendFriendList(session);
		break;
	}

	case C2S_REQUEST_RANKING: {
		server.SendRanking(session);
		break;
	}

	case C2S_FAST_MATCHING: {
		HandleFastMatchingPacket(packet, session);
		break;
	}

	case C2S_REQUEST_FRIEND: {
		HandleRequestFriendPacket(packet, session);
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
