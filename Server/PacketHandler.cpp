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
	C2S_LOGIN_PACKET* recv_p = reinterpret_cast<C2S_LOGIN_PACKET*>(packet);
	SessionKey key;
	{
		std::lock_guard<std::mutex> lock(session.GetMutex());
		if (session.GetSessionKey().gen != request_gen) return;
		if (session.GetState() != SESS_STATE::LOGIN) return;
		key = session.GetSessionKey();
	}

	std::string login_id = server.CharBufToString(recv_p->login_id, sizeof(recv_p->login_id));
	std::string password = server.CharBufToString(recv_p->login_password, sizeof(recv_p->login_password));
	Database& repr_db = server.GetDB();
	auto task_login = [&repr_db, key, login_id, password]() {
		repr_db.ExecuteLogin(key, login_id, password);
		};

	repr_db.Enqueue(task_login);
}

void PacketHandler::HandleMessagePacket(char* packet, Session& session, int request_gen)
{
	C2S_MESSAGE_PACKET* recv_p = reinterpret_cast<C2S_MESSAGE_PACKET*>(packet);
	int msg_size = recv_p->header.size - sizeof(C2S_MESSAGE_PACKET);
	if (msg_size == 0) return;
	int send_p_size = sizeof(S2C_MESSAGE_PACKET) + msg_size;

	std::string nickname;
	int id;
	{
		std::lock_guard<std::mutex> lock(session.GetMutex());
		if (session.GetSessionKey().gen != request_gen) return;
		if (session.GetState() != SESS_STATE::LOBBY) return;
		nickname = session.GetDBInfo().nickname;
		id = session.GetDBInfo().id;
	}

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

void PacketHandler::HandleTestPacket(char* packet, Session& session)
{
	C2S_TEST_PACKET* recv_p = reinterpret_cast<C2S_TEST_PACKET*>(packet);
	char* send_p = new char[recv_p->header.size];
	int msg_size = recv_p->header.size - sizeof(C2S_TEST_PACKET);
	S2C_TEST_PACKET front_p;
	front_p.header.size = recv_p->header.size;
	front_p.header.type = S2C_TEST;
	front_p.id = session.GetDBInfo().id;
	front_p.last_time = recv_p->last_time;
	memcpy(send_p, &front_p, sizeof(S2C_TEST_PACKET));
	memcpy(send_p + sizeof(S2C_TEST_PACKET), reinterpret_cast<char*>(recv_p) + sizeof(C2S_TEST_PACKET), msg_size);

	server.BroadCastToLobby(send_p);

	delete[] send_p;
}

void PacketHandler::HandleDisconnectPacket(Session& session)
{
	session.StoreDisconnectFlag(true);
	server.Disconnect(session.GetSessionKey());
}

void PacketHandler::HandleJoinOpenRoomPacket(char* packet, Session& session, int request_gen)
{
	C2S_JOIN_OPEN_ROOM_PACKET* join_p = reinterpret_cast<C2S_JOIN_OPEN_ROOM_PACKET*>(packet);
	server.TryJoinRoom(session, request_gen, join_p->room_gen, "");
}

void PacketHandler::HandleJoinLockRoomPacket(char* packet, Session& session, int request_gen)
{
	C2S_JOIN_LOCK_ROOM_PACKET* join_p = reinterpret_cast<C2S_JOIN_LOCK_ROOM_PACKET*>(packet);
	server.TryJoinRoom(session, request_gen, join_p->room_gen, server.CharBufToString(join_p->room_password, sizeof(join_p->room_password)));
}

void PacketHandler::HandleFastMatchingPacket(char* packet, Session& session, int request_gen)
{
	C2S_FAST_MATCHING_PACKET* matching_p = reinterpret_cast<C2S_FAST_MATCHING_PACKET*>(packet);
	server.FindMatch(session, request_gen, matching_p->max_user);
}

void PacketHandler::HandleRequestFriendPacket(char* packet, Session& session, int request_gen)
{
	C2S_REQUEST_FRIEND_PACKET* friend_p = reinterpret_cast<C2S_REQUEST_FRIEND_PACKET*>(packet);

	Session& requester_sess = session;
	FriendInfo requester_info;
	{
		std::lock_guard<std::mutex> lock(requester_sess.GetMutex());
		if (requester_sess.GetState() != SESS_STATE::LOBBY) return;
		if (requester_sess.GetSessionKey().gen == request_gen) {
			requester_info.id = requester_sess.GetDBInfo().id;
			requester_info.nickname = requester_sess.GetDBInfo().nickname;
		}
		else return;
	}

	int recver_id = friend_p->recver_id;
	Database& repr_db = server.GetDB();
	auto task_afr = [&repr_db, requester_info, recver_id]() {
		repr_db.ExecuteAddFriendRequest(requester_info, recver_id);
		};

	repr_db.Enqueue(task_afr);
}

void PacketHandler::HandleAcceptFriendPacket(char* packet, Session& session, int request_gen)
{
	C2S_ACCEPT_FRIEND_PACKET* accept_p = reinterpret_cast<C2S_ACCEPT_FRIEND_PACKET*>(packet);
	Session& accepter_session = session;

	FriendInfo accepter_info;
	{
		std::lock_guard<std::mutex> lock(accepter_session.GetMutex());
		if (accepter_session.GetState() != SESS_STATE::LOBBY) return;
		if (accepter_session.GetSessionKey().gen == request_gen) {
			accepter_info.id = accepter_session.GetDBInfo().id;
			accepter_info.nickname = accepter_session.GetDBInfo().nickname;
		}
		else return;
	}

	int requester_id = accept_p->requester_id;
	Database& repr_db = server.GetDB();
	auto task_af = [&repr_db, requester_id, accepter_info]() {
		repr_db.ExecuteAddFriend(requester_id, accepter_info);
		};

	repr_db.Enqueue(task_af);
}

void PacketHandler::HandleDeleteFriendPacket(char* packet, Session& session, int request_gen)
{
	C2S_DELETE_FRIEND_PACKET* delete_p = reinterpret_cast<C2S_DELETE_FRIEND_PACKET*>(packet);
	Session& requester_session = session;
	int target_id = delete_p->target_id;
	int requester_id = -1;
	{
		std::lock_guard<std::mutex> lock(requester_session.GetMutex());
		if (requester_session.GetState() != SESS_STATE::LOBBY) return;
		if (requester_session.GetSessionKey().gen == request_gen) {
			requester_id = requester_session.GetDBInfo().id;
		}
		else return;
	}
	Database& repr_db = server.GetDB();
	auto task_df = [&repr_db, requester_id, target_id]() {
		repr_db.ExecuteDeleteFriend(requester_id, target_id);
		};

	repr_db.Enqueue(task_df);
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
