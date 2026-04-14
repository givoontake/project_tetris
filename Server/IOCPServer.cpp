#include <iostream>
#include <algorithm>
#include "IOCPServer.h"
#include "SingleRoom.h"
#include "MultiRoom.h"

#undef min

IOCPServer::IOCPServer()
{
	for (int i = 0; i < MAX_USER; ++i) {
		users[i].SetIndex(i);
	}

	// load는 객체 복사가 아니라 컨트롤 블록을 가리키는 핸들(shared_ptr)만 복사하는 것, 접근 흐름은 shared_ptr -> controll block(카운터, 실제 객체 포인터 등 존재) -> 실제 객체 이다.
	// 즉 참조 카운트를 늘리는 동작이며 다른 곳에서 객체를 해제해도 안전하게 동작할 수 있도록 한다. 의도된 동작은 아닐 수 있어도 수명은 확실하게 관리된다.
	// shared_ptr의 기본값은 nullptr이므로 초기화는 필요 없다.
	
	//for (int i = 0; i < MAX_ROOM; ++i) {

	//	auto room = rooms[i].load(); 
	//	room = nullptr;
	//}

	//packet_handler = std::make_unique<PacketHandler>(this);
	//for (auto& user : users) {
	//	user = std::make_unique<Session>(packet_handler.get()); // packet_handler는 unique_ptr이므로 get()을 이용해 raw ptr을 넘긴다.
	//}

	WSAStartup(MAKEWORD(2, 2), &wsadata);

	listen_socket = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);
	client_socket = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);
	memset(&server_addr, 0, sizeof(server_addr));
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(PORT_NUM);
	server_addr.sin_addr.S_un.S_addr = INADDR_ANY;

	accept_over.SetOperationType(OP_TYPE::ACCEPT);

}

IOCPServer::~IOCPServer()
{
	for(auto& room : rooms) {
		std::atomic_store(&room, std::shared_ptr<TetrisRoom>{}); // nullptr과 같은 논리
	}
	active_rooms.Clear();
	active_users.Clear();
	closesocket(listen_socket);
	closesocket(client_socket);
	db.SetRunning(false);
	WSACleanup();
}

void IOCPServer::HandleLoginPacket(char* packet, Session& session, int request_gen)
{
	C2S_LOGIN_PACKET* recv_p = reinterpret_cast<C2S_LOGIN_PACKET*>(packet);
	SessionKey key;
	{
		std::lock_guard<std::mutex> lock(session.GetMutex());
		if (session.GetSessionKey().gen != request_gen) return;
		if (session.GetState() != SESS_STATE::LOGIN) return;
		key = session.GetSessionKey();
	}

	std::string login_id = CharBufToString(recv_p->login_id, sizeof(recv_p->login_id));
	std::string password = CharBufToString(recv_p->login_password, sizeof(recv_p->login_password));
	Database& repr_db = db;
	auto task_login = [&repr_db, key, login_id, password]() {
		repr_db.ExecuteLogin(key, login_id, password);
		};

	db.Enqueue(task_login);
}

void IOCPServer::HandleMessagePacket(char* packet, Session& session, int request_gen)
{
	C2S_MESSAGE_PACKET* recv_p = reinterpret_cast<C2S_MESSAGE_PACKET*>(packet);
	int msg_size = recv_p->size - sizeof(C2S_MESSAGE_PACKET);
	if (msg_size == 0) return;
	int send_p_size = sizeof(S2C_MESSAGE_PACKET) + msg_size;
	char* send_p = new char[send_p_size];

	std::string nickname;
	int id;
	{
		std::lock_guard<std::mutex> lock(session.GetMutex());
		if (session.GetSessionKey().gen != request_gen) return;
		if (session.GetState() != SESS_STATE::LOBBY) return;
		nickname = session.GetDBInfo().nickname;
		id = session.GetDBInfo().id;
	}

	S2C_MESSAGE_PACKET front_p;
	front_p.size = send_p_size;
	front_p.type = S2C_MESSAGE;
	front_p.id = id;
	StringToCharBuf(nickname, front_p.user_name, sizeof(front_p.user_name));
	memcpy(send_p, &front_p, sizeof(S2C_MESSAGE_PACKET));
	memcpy(send_p + sizeof(S2C_MESSAGE_PACKET), reinterpret_cast<char*>(recv_p) + sizeof(C2S_MESSAGE_PACKET), msg_size);

	BroadCastToLobby(send_p);

	delete[] send_p;
}

void IOCPServer::HandleTestPacket(char* packet, Session& session)
{
	C2S_TEST_PACKET* recv_p = reinterpret_cast<C2S_TEST_PACKET*>(packet);
	char* send_p = new char[recv_p->size];
	int msg_size = recv_p->size - sizeof(C2S_TEST_PACKET);
	S2C_TEST_PACKET front_p;
	front_p.size = recv_p->size;
	front_p.type = S2C_TEST;
	front_p.id = session.GetDBInfo().id;
	front_p.last_time = recv_p->last_time;
	memcpy(send_p, &front_p, sizeof(S2C_TEST_PACKET));
	memcpy(send_p + sizeof(S2C_TEST_PACKET), reinterpret_cast<char*>(recv_p) + sizeof(C2S_TEST_PACKET), msg_size);

	BroadCastToLobby(send_p);

	delete[] send_p;
}

void IOCPServer::HandleDisconnectPacket(Session& session)
{
	session.StoreDisconnectFlag(true);
	Disconnect(session.GetSessionKey().index);
}

void IOCPServer::HandleJoinOpenRoomPacket(char* packet, Session& session, int request_gen)
{
	C2S_JOIN_OPEN_ROOM_PACKET* join_p = reinterpret_cast<C2S_JOIN_OPEN_ROOM_PACKET*>(packet);
	TryJoinRoom(session, request_gen, join_p->room_gen, nullptr);
}

void IOCPServer::HandleJoinLockRoomPacket(char* packet, Session& session, int request_gen)
{
	C2S_JOIN_LOCK_ROOM_PACKET* join_p = reinterpret_cast<C2S_JOIN_LOCK_ROOM_PACKET*>(packet);
	TryJoinRoom(session, request_gen, join_p->room_gen, join_p->room_password);
}

void IOCPServer::HandleFastMatchingPacket(char* packet, Session& session, int request_gen)
{
	C2S_FAST_MATCHING_PACKET* matching_p = reinterpret_cast<C2S_FAST_MATCHING_PACKET*>(packet);
	FindMatch(session, request_gen, matching_p->max_user);
}

void IOCPServer::HandleRequestFriendPacket(char* packet, Session& session, int request_gen)
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
	Database& repr_db = db;
	auto task_afr = [&repr_db, requester_info, recver_id]() {
		repr_db.ExecuteAddFriendRequest(requester_info, recver_id);
		};

	db.Enqueue(task_afr);
}

void IOCPServer::HandleAcceptFriendPacket(char* packet, Session& session, int request_gen)
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
	Database& repr_db = db;
	auto task_af = [&repr_db, requester_id, accepter_info]() {
		repr_db.ExecuteAddFriend(requester_id, accepter_info);
		};

	db.Enqueue(task_af);
}

void IOCPServer::HandleDeleteFriendPacket(char* packet, Session& session, int request_gen)
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
	Database& repr_db = db;
	auto task_df = [&repr_db, requester_id, target_id]() {
		repr_db.ExecuteDeleteFriend(requester_id, target_id);
		};

	db.Enqueue(task_df);
}

void IOCPServer::HandlePacket(char* packet, Session& session, int request_gen)
{
	// 작업에 필요한 데이터는 락으로 잡고 전송에 필요한 본인 정보만 복사(전송에 필요한 본인 정보를 읽을 때 연결이 끊기면 데이터 레이스 발생 가능)
	switch (packet[2]) {

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
		CreateOpenRoom(packet, session, request_gen);
		break;
	}

	case C2S_ADD_LOCK_ROOM: {
		CreateLockRoom(packet, session, request_gen);
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
		SendRoomList(session, request_gen);
		break;
	}
	case C2S_REQUEST_LOBBY_USER_LIST: {
		SendLobbyUserList(session, request_gen);
		break;
	}
	case C2S_REQUEST_FRIEND_LIST: {
		SendFriendList(session, request_gen);
		break;
	}
	case C2S_REQUEST_RANKING: {
		SendRanking(session, request_gen);
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
void IOCPServer::SendRoomList(Session& session, int request_gen)
{
	// 더미 방 생성
	for (int i = 0; i < 20; ++i) {
		S2C_ROOM_INFO_PACKET p{};
		p.size = sizeof(S2C_ROOM_INFO_PACKET);
		p.type = S2C_ROOM_INFO;

		p.room_gen = 1,000,000 + i;

		std::string name = "DummyRoom_" + std::to_string(i);
		StringToCharBuf(name, p.room_name, sizeof(p.room_name));

		p.max_user = 2;
		p.cur_user = 1; 

		p.is_private = false;
		p.is_play = false;

		session.SendPacket(request_gen, reinterpret_cast<char*>(&p), iocp_handle);
	}

	{
		// 어차피 send의 세션 조건에서 걸러지지만, 방이 많아지면 작업 자체가 길어질 수 있으므로 미리 체크
		std::lock_guard<std::mutex> lock(session.GetMutex());
		if (session.GetSessionKey().gen != request_gen) return;
		if (session.GetState() != SESS_STATE::LOBBY) return;
	}
	
	char packet_buf[BUF_SIZE];
	int packet_size = 0;
	for (auto& room : rooms) {
		auto room_sp = room.load(); // 먼저 참조 카운트를 늘려야함
		if (!room_sp) continue;
		S2C_ROOM_INFO_PACKET info_p;
		if (room_sp->GetRoomState() == ROOM_STATE::EMPTY) continue;
		info_p.size = sizeof(S2C_ROOM_INFO_PACKET);
		info_p.type = S2C_ROOM_INFO;
		info_p.room_gen = room_sp->GetRoomGen();
		const char* name = room_sp->GetRoomName();
		info_p.max_user = room_sp->GetMaxUser();
		info_p.cur_user = room_sp->GetCurrentUser();
		memcpy(&info_p.room_name, name, MAX_USER_NAME);
		info_p.is_private = room_sp->GetIsPrivate();
		bool is_play;
		if (room_sp->GetRoomState() == ROOM_STATE::WAIT) is_play = false;
		else is_play = true;
		info_p.is_play = is_play;

		if (packet_size + sizeof(info_p) > BUF_SIZE) { 
			session.SendBoundPacket(request_gen, reinterpret_cast<char*>(packet_buf), packet_size, iocp_handle);
			packet_size = 0;
		}

		memcpy(packet_buf + packet_size, &info_p, sizeof(info_p));
		packet_size += sizeof(info_p);
		//std::lock_guard<std::mutex> lock(session->GetMutex()); // 조건 불일치시 바로 리턴을 위해 체크만 하자, sendpacket()내에 뮤텍스 있어서 넣으면 데드락
		//if (session->GetSessionKey().gen != request_gen) return;
		//if (session->GetState() == SESS_STATE::NONE) return;
	}
	session.SendBoundPacket(request_gen, reinterpret_cast<char*>(packet_buf), packet_size, iocp_handle);
}

// 
bool IOCPServer::TryJoinRoom(Session& session, int request_gen, int room_gen, const char* room_password)
{	
	int result = ERROR_CODE::ROOM_NOT_FOUND;
	int room_index = FindRoom(room_gen);
	if (room_index != -1) {
		std::shared_ptr<TetrisRoom> room_sp = rooms[room_index].load();
		if (room_sp) {
			std::lock_guard<std::mutex> lock(room_sp->GetRoomMutex());
			if (room_sp->GetRoomState() == ROOM_STATE::WAITING_DELETE) result = ERROR_CODE::ROOM_NOT_FOUND;
			else {
				if (room_sp->GetMaxUser() != 1) { // 싱글이 아닌 경우
					auto multi_sp = std::dynamic_pointer_cast<MultiRoom>(room_sp); // TetrisRoom -> MultiRoom으로 다운캐스팅(참조 카운트 증가)
					if (!multi_sp) result = ERROR_CODE::SERVER_ERROR;
					else {
						result = multi_sp->AddUser(session, request_gen, room_password); // 멀티 룸에만 있는 함수라 위에서 다운캐스팅 한 것
					}
				}
				else {
					result = ERROR_CODE::INVALID_REQUEST;
				}
			}
		}
		else {
			result = ERROR_CODE::ROOM_NOT_FOUND;
		}
	}

	if (result == SUCCESS) return true;

	SendError(session, request_gen, result);
	return false;
}

int IOCPServer::FindRoom(int room_gen)
{
	return active_rooms.FindRoomIndex(room_gen);
}

int IOCPServer::FindUser(int user_id)
{
	for (int i = 0; i < users.size(); ++i) { // 다른 세션 찾는데 락을 걸어버리면 좀 이상하다. 
		if (users[i].GetDBInfo().id == user_id) return i;
	}

	return -1;
}

void IOCPServer::SendError(Session& session, int request_gen, int error_code)
{
	S2C_ERROR_PACKET error_p;
	error_p.size = sizeof(S2C_ERROR_PACKET);
	error_p.type = S2C_ERROR;
	error_p.error_code = error_code;

	session.SendPacket(request_gen, reinterpret_cast<char*>(&error_p), iocp_handle);
}

bool IOCPServer::CheckDuplicateLoginId(const std::string& login_id)
{
	for (auto& user : users) {
		std::lock_guard<std::mutex> lock(user.GetMutex());
		if (user.GetState() == SESS_STATE::NONE) continue;
		if (user.GetDBInfo().login_id == login_id) return true;
	}

	return false;
}

void IOCPServer::FindMatch(Session& session, int request_gen, int max_user)
{
	if (max_user == 0) {
		for (auto& room : rooms) {
			// 방에 접근할 때는 무조건 Shared_ptr을 로드해서 참조 카운트를 늘려야 한다. 방이 삭제되더라도 안전하게 동작하기 위해서이다.
			// 단순히 널을 체크하고 들어가도 그 다음 내부 객체 접근 시 그 객체가 삭제되었을 수 있다.
			auto room_sp = room.load();
			if (room_sp) {
				if (room_sp->GetMaxUser() == 2 or room_sp->GetMaxUser() == 5) { // 공개 멀티 방 중 아무 방이나 찾기
					if (TryJoinRoom(session, request_gen, room_sp->GetRoomGen(), nullptr)) return;
				}
			}
		}
	}

	else if (max_user == 2) {
		for (auto& room : rooms) {
			auto room_sp = room.load();
			if (room_sp) {
				if (room_sp->GetMaxUser() == max_user) {
					if (TryJoinRoom(session, request_gen, room_sp->GetRoomGen(), nullptr)) return;
				}
			}
		}
	}

	else if (max_user == 5) {
		for (auto& room : rooms) {
			auto room_sp = room.load();
			if (room_sp) {
				if (room_sp->GetMaxUser() == max_user) {
					if (TryJoinRoom(session, request_gen, room_sp->GetRoomGen(), nullptr)) return;
				}
			}
		}
	}

	else SendError(session, request_gen, ERROR_CODE::INVALID_REQUEST);
	
	SendError(session, request_gen, ERROR_CODE::NOT_FOUND_JOINABLE_ROOM);
}

void IOCPServer::SendLobbyUserList(Session& session, int request_gen)
{
	{
		// 비용을 줄이기 위한 선체크
		std::lock_guard<std::mutex> lock(session.GetMutex());
		if (session.GetSessionKey().gen != request_gen) return;
		if (session.GetState() != SESS_STATE::LOBBY) return;
	}
	
	int packet_size = 0;
	char packet_buf[BUF_SIZE];
	for (auto& user : users) {
		S2C_LOBBY_USER_INFO_PACKET info_p;
		info_p.size = sizeof(S2C_LOBBY_USER_INFO_PACKET);
		info_p.type = S2C_LOBBY_USER_INFO;
		//info_p.user_id = -1;
		{
			std::lock_guard<std::mutex> lock(user.GetMutex());
			if (user.GetDBInfo().id == session.GetDBInfo().id) continue;
			if (user.GetState() == SESS_STATE::LOBBY) {
				info_p.user_id = user.GetDBInfo().id;
				StringToCharBuf(user.GetDBInfo().nickname, info_p.nickname, MAX_ROOM_NAME);
			}
			else continue;
		}
		
		if (packet_size + sizeof(info_p) > BUF_SIZE) {
			session.SendBoundPacket(request_gen, reinterpret_cast<char*>(packet_buf), packet_size, iocp_handle);
			packet_size = 0;
		}

		memcpy(packet_buf + packet_size, &info_p, sizeof(info_p));
		packet_size += sizeof(info_p);
	}
	session.SendBoundPacket(request_gen, reinterpret_cast<char*>(packet_buf), packet_size, iocp_handle);
}

void IOCPServer::SendFriendList(Session& session, int request_gen)
{
	std::vector<FriendInfo> friend_list;
	{
		std::lock_guard<std::mutex> lock(session.GetMutex());
		if (session.GetSessionKey().gen != request_gen) return;
		if (session.GetState() != SESS_STATE::LOBBY) return;
		friend_list = session.GetFriendList();
	}

	int packet_size = 0;
	char packet_buf[BUF_SIZE];
	for (auto& friend_info : friend_list) {
		S2C_FRIEND_INFO_PACKET info_p; // 얘는 그냥 지 세션에 있는 친구 목록이라 미리 다 작성하고 현재 친구 상태만 검사해서 보내주면 됨
		info_p.size = sizeof(S2C_FRIEND_INFO_PACKET);
		info_p.type = S2C_FRIEND_INFO;
		info_p.user_id = friend_info.id;
		info_p.is_lobby = false;
		StringToCharBuf(friend_info.nickname, info_p.nickname, MAX_USER_NAME);

		// 로비인지 체크만 함
		int sess_index = FindSessionIndexById(friend_info.id);
		if (sess_index != -1) {
			Session& sess = users[sess_index];
			std::lock_guard<std::mutex> lock(sess.GetMutex());
			if (sess.GetDBInfo().id != friend_info.id) continue;
			if (sess.GetState() == SESS_STATE::LOBBY) info_p.is_lobby = true;
		}

		if (packet_size + sizeof(info_p) > BUF_SIZE) {
			session.SendBoundPacket(request_gen, reinterpret_cast<char*>(packet_buf), packet_size, iocp_handle);
			packet_size = 0;
		}

		memcpy(packet_buf + packet_size, &info_p, sizeof(info_p));
		packet_size += sizeof(info_p);
	}
	session.SendBoundPacket(request_gen, reinterpret_cast<char*>(packet_buf), packet_size, iocp_handle);
}

void IOCPServer::SendRanking(Session& session, int request_gen)
{
	{
		std::lock_guard<std::mutex> lock(session.GetMutex());
		if (session.GetSessionKey().gen != request_gen) return;
		if (session.GetState() != SESS_STATE::LOBBY) return;
	}

	std::vector<RankingInfo> rankings = ranking_manager.GetRankings();
	if (rankings.empty()) return;

	int packet_size = 0;
	char packet_buf[BUF_SIZE];
	for (const auto& ranking : rankings) {
		S2C_RANKING_INFO_PACKET info_p{};
		info_p.size = sizeof(S2C_RANKING_INFO_PACKET);
		info_p.type = S2C_RANKING_INFO;
		StringToCharBuf(ranking.nickname, info_p.nickname, MAX_USER_NAME);
		info_p.score = ranking.score;

		if (packet_size + sizeof(info_p) > BUF_SIZE) {
			session.SendBoundPacket(request_gen, reinterpret_cast<char*>(packet_buf), packet_size, iocp_handle);
			packet_size = 0;
		}

		memcpy(packet_buf + packet_size, &info_p, sizeof(info_p));
		packet_size += sizeof(info_p);
	}

	session.SendBoundPacket(request_gen, reinterpret_cast<char*>(packet_buf), packet_size, iocp_handle);
}

void IOCPServer::SendAddFriendResult(FriendInfo& requester_info, FriendInfo& accepter_info)
{
	int requester_index = FindSessionIndexById(requester_info.id);
	int accepter_index = FindSessionIndexById(accepter_info.id);

	int requester_gen = -1;
	S2C_ADD_FRIEND_PACKET add_p;
	add_p.size = sizeof(S2C_ADD_FRIEND_PACKET);
	add_p.type = S2C_ADD_FRIEND;

	if (requester_index != -1) {
		Session& requester_sess = users[requester_index];
		std::lock_guard<std::mutex> lock(requester_sess.GetMutex());
		if (requester_sess.GetDBInfo().id == requester_info.id) {
			requester_sess.AddFriend(accepter_info);
			if ((requester_sess.GetState() == SESS_STATE::LOBBY)) requester_gen = requester_sess.GetSessionKey().gen;
		}
	}

	if (requester_gen != -1) {
		Session& requester_sess = users[requester_index];
		add_p.friend_id = accepter_info.id;
		StringToCharBuf(accepter_info.nickname, add_p.friend_nickname, MAX_USER_NAME);
		requester_sess.SendPacket(requester_gen, reinterpret_cast<char*>(&add_p), iocp_handle);
	}
	
	int accepter_gen = -1;
	if (accepter_index != -1){
		Session& accepter_sess = users[accepter_index];
		std::lock_guard<std::mutex> lock(accepter_sess.GetMutex());
		if (accepter_sess.GetDBInfo().id == accepter_info.id) {
			accepter_sess.AddFriend(requester_info);
			if (accepter_sess.GetState() == SESS_STATE::LOBBY) accepter_gen = accepter_sess.GetSessionKey().gen;
		}
	}

	if (accepter_gen != -1) {
		Session& accepter_sess = users[accepter_index];
		add_p.friend_id = requester_info.id;
		StringToCharBuf(requester_info.nickname, add_p.friend_nickname, MAX_USER_NAME);
		accepter_sess.SendPacket(accepter_gen, reinterpret_cast<char*>(&add_p), iocp_handle);
	}
}

// DB는 변경이 성공한 경우에 작업 성공으로 판정하도록 했으므로 결과를 통지할 세션의 존재 유무만 체크하면 된다.
void IOCPServer::SendDeleteFriendResult(int requester_id, int target_id)
{
	// 세션은 재사용하므로 논리적으로는 포인터는 항상 유효
	int requester_index = FindSessionIndexById(requester_id);
	int target_index = FindSessionIndexById(target_id);

	int requester_gen = -1;
	S2C_DELETE_FRIEND_PACKET delete_p;
	delete_p.size = sizeof(S2C_DELETE_FRIEND_PACKET);
	delete_p.type = S2C_DELETE_FRIEND;

	if (requester_index != -1){ // 안전성 + 가드
		Session& requester_sess = users[requester_index];
		std::lock_guard<std::mutex> lock(requester_sess.GetMutex());
		if (requester_sess.GetDBInfo().id == requester_id) {
			requester_sess.DeleteFriend(target_id);
			if (requester_sess.GetState() == SESS_STATE::LOBBY) requester_gen = requester_sess.GetSessionKey().gen;
		}
	}

	if (requester_gen != -1) {
		Session& requester_sess = users[requester_index];
		delete_p.target_id = target_id;
		requester_sess.SendPacket(requester_gen, reinterpret_cast<char*>(&delete_p), iocp_handle);
	}
	
	int target_gen = -1;
	if (target_index != -1){
		Session& target_sess = users[target_index];
		std::lock_guard<std::mutex> lock(target_sess.GetMutex());
		if (target_sess.GetDBInfo().id == target_id) {
			target_sess.DeleteFriend(requester_id);
			if (target_sess.GetState() == SESS_STATE::LOBBY)  target_gen = target_sess.GetSessionKey().gen;
		}
	}

	if (target_gen != -1) {
		Session& target_sess = users[target_index];
		delete_p.target_id = requester_id;
		target_sess.SendPacket(target_gen, reinterpret_cast<char*>(&delete_p), iocp_handle);
	}
}

void IOCPServer::StartServer()
{
	bind(listen_socket, reinterpret_cast<sockaddr*>(&server_addr), sizeof(server_addr));
	listen(listen_socket, SOMAXCONN);
	iocp_handle = CreateIoCompletionPort(INVALID_HANDLE_VALUE, 0, 0, 0);
	CreateIoCompletionPort(reinterpret_cast<HANDLE>(listen_socket), iocp_handle, 9999, 0);
	int addr_size = sizeof(SOCKADDR_IN);
	AcceptEx(listen_socket, client_socket, accept_over.packet_buf, 0, addr_size + 16, addr_size + 16, 0, &accept_over.ex_over.over);
}

void IOCPServer::ProcessGQCS()
{
	while (is_running){
		DWORD transferred_bytes = 0;
		ULONG_PTR completion_key = 0;
		WSAOVERLAPPED* over = nullptr;
		BOOL result = GetQueuedCompletionStatus( // 인자로 넘긴 주소 변수의 값을 채워준다.
			iocp_handle,
			&transferred_bytes,
			&completion_key,
			&over,
			INFINITE);

		ExOverlapped* ex_over = reinterpret_cast<ExOverlapped*>(over);
		Session& session = users[completion_key];

		if (!result){
			if (ex_over->op_type == OP_TYPE::ACCEPT) std::cout << "Accept Error" << WSAGetLastError() << "\n";
			else { // 클라이언트 강제 종료일 경우
				session.StoreDisconnectFlag(true);
				Disconnect(static_cast<int>(session.GetSessionKey().index));
				if (ex_over->op_type == OP_TYPE::SEND) delete ex_over;
			}
			continue;
		}

		// 클라이언트 정상 종료일 경우
		if (transferred_bytes == 0 && ex_over->op_type != OP_TYPE::ACCEPT) {
			session.StoreDisconnectFlag(true);
			Disconnect(static_cast<int>(session.GetSessionKey().index));
			if (ex_over->op_type == OP_TYPE::SEND) delete ex_over;
			continue;
		}

		switch (ex_over->op_type) {
		case OP_TYPE::ACCEPT: {
			IOOverlapped* io_over = reinterpret_cast<IOOverlapped*>(ex_over);
			int new_index = GetEmptyUserIndex();
			if (new_index != -1) {
				int gen = GetNewUserGen();
				users[new_index].InitSession(gen, client_socket);
				CreateIoCompletionPort(reinterpret_cast<HANDLE>(client_socket), iocp_handle, new_index, 0);
				users[new_index].RecvPacket(users[new_index].GetSessionKey().gen, iocp_handle);
				client_socket = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED); // 커널 내 새 소켓을 생성하고 그것을 가리키는 핸들을 받음, 기존 핸들은 이미 initsession 되어 세션 내부에 가지고 있다.
				std::cout << "Session[" << new_index << "] connect/Gen: " << users[new_index].GetSessionKey().gen << std::endl;
			}

			else {
				std::cout << "서버가 혼잡합니다. 연결을 종료합니다.\n";
				closesocket(client_socket);
				client_socket = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED); // WSASocket은 리소스 부족 시 실패할 수 있다. 실패 시 INVALID_SOCKET 반환
			}

			// 리소스 부족으로 실패하면 일단 연결 더 안받는걸로 하자.
			if (client_socket != INVALID_SOCKET) {
				ZeroMemory(&accept_over.ex_over.over, sizeof(accept_over.ex_over.over));
				int addr_size = sizeof(SOCKADDR_IN);
				// AcceptEx도 리소스 부족으로 실패할 수 있다. 
				bool res = AcceptEx(listen_socket, client_socket, accept_over.packet_buf, 0, addr_size + 16, addr_size + 16, 0, &accept_over.ex_over.over);
				if (!res && WSAGetLastError() != ERROR_IO_PENDING) std::cerr << "AcceptEx fail.. " << std::endl;
			}
			break;
		}

		case OP_TYPE::RECV: {
			IOOverlapped* io_over = reinterpret_cast<IOOverlapped*>(ex_over);
			int request_gen = io_over->ex_over.request_gen;
			ProcessPacket(users[completion_key], request_gen, transferred_bytes);
			users[completion_key].RecvPacket(request_gen, iocp_handle);
			
			break;
		}

		case OP_TYPE::SEND: {
			IOOverlapped* io_over = reinterpret_cast<IOOverlapped*>(ex_over);
			delete io_over;

			break;
		}
		case OP_TYPE::DELETE_ROOM: // 이 작업이 올 때 키는 방 인덱스임
			delete ex_over;
			DeleteRoom(static_cast<int>(completion_key));

			break;
		case OP_TYPE::DB:
			DBOverlapped* db_over = reinterpret_cast<DBOverlapped*>(ex_over);
			if (db_over->type == DBOperationType::LOAD_RANKING) ProcessRankingResult(db_over);
			else ProcessDBResult(db_over, session, db_over->ex_over.request_gen);

			break;
		}
	}
}

void IOCPServer::ProcessPacket(Session& session, int request_gen, int recv_bytes)
{   
	short packet_size;
	int offset = 0;
	char p_buffer[BUF_SIZE];
	int remain_data_size = 0;

	// 작업에 사용해야 할 세션 내의 값들을 세션 락을 걸고 안전하게 복사해온다.
	// 복사한 값을 그 다음에 처리하는 것은 문제 없다. 처리 도중 재사용된다고 해도 어차피 같은 세션인지 계속 검증하므로 걸러진다
	// 작업을 완료하면 세션에 반영되어야 하는 remain_data_size만 락을 걸고 세팅한다.
	{
		std::lock_guard<std::mutex> lock(session.GetMutex());

		if (session.GetState() == SESS_STATE::NONE) return;
		if (session.GetSessionKey().gen != request_gen) return;

		if (recv_bytes + session.GetRemainDataSize() > BUF_SIZE) {
			PostQueuedCompletionStatus(iocp_handle, 0, request_gen, nullptr);
			return;
		}
		
		else session.AddDataSize(recv_bytes);

		if (session.GetRemainDataSize() < sizeof(short)) return;

		remain_data_size = session.GetRemainDataSize();
		memcpy(&packet_size, session.GetExOver().packet_buf, sizeof(packet_size));
		memcpy(p_buffer, session.GetExOver().packet_buf, remain_data_size);
	}

	while (remain_data_size - offset >= packet_size)
	{
		char* packet = new char[packet_size];
		memcpy(packet, p_buffer + offset, packet_size);
		RoutePacket(packet, session, request_gen);
		delete[] packet;

		offset += packet_size;
		if (remain_data_size - offset < sizeof(short)) break;
		memcpy(&packet_size, p_buffer + offset, sizeof(packet_size));
	}

	{
		std::lock_guard<std::mutex> lock(session.GetMutex());
		if (session.GetState() == SESS_STATE::NONE) return;
		if (session.GetSessionKey().gen != request_gen) return;
		session.AddDataSize(-offset);
		memmove(session.GetExOver().packet_buf, session.GetExOver().packet_buf + offset, session.GetRemainDataSize());
	}
}

void IOCPServer::RoutePacket(char* packet, Session& session, int request_gen)
{
	PrintPacketType(packet[2]);
	switch (session.GetState()) {
	case SESS_STATE::NONE:
		return;
	case SESS_STATE::LOGIN:
		HandlePacket(packet, session, request_gen);
		break;
	case SESS_STATE::LOBBY:
		HandlePacket(packet, session, request_gen);
		break;
	case SESS_STATE::ROOM: {
		auto room_ptr = rooms[session.GetRoomIndex()].load();
		if (room_ptr) room_ptr->HandlePacket(packet, session); // 방에 들어가있는 상태라면 내부에서 세션은 키 없이도 안전하게 관리된다.
		break;
	}
	}

}

void IOCPServer::BroadCastToLobby(char* packet)
{
	for (auto& user : users) {
		if (user.GetState() == SESS_STATE::LOBBY) {
			user.SendPacket(packet, iocp_handle);
		}
	}
}

//void IOCPServer::SendToSelf(char* packet, int self_index)
//{
//	users[self_index]->SendPacket(packet, iocp_handle);
//}

void IOCPServer::CreateOpenRoom(char* packet, Session& session, int request_gen)
{
	C2S_ADD_OPEN_ROOM_PACKET* open_p = reinterpret_cast<C2S_ADD_OPEN_ROOM_PACKET*>(packet);
	OpenRoomInitData data;
	//std::shared_ptr<TetrisRoom> new_room;
	bool is_single;
	if (open_p->max_user == 1) {
		data.max_user = open_p->max_user;
		is_single = true;
	}
		
	else if (open_p->max_user == 2 || open_p->max_user == 5) {
		data.max_user = open_p->max_user;
		is_single = false;
	}
		
	else return;
	memcpy(data.room_name, open_p->room_name, sizeof(data.room_name));
	data.room_gen = GetNewRoomGen();
	
	std::shared_ptr<TetrisRoom> new_room;
	
	for (int i = 0; i < MAX_ROOM; ++i) {
		if (rooms[i].load() == nullptr) {
			data.room_index = i;
			{
				std::lock_guard<std::mutex> lock(session.GetMutex());
				if (session.GetState() == SESS_STATE::NONE) return;
				if (session.GetSessionKey().gen != request_gen) return;

				if (is_single) new_room = std::make_shared<SingleRoom>(this, session, data);
				else new_room = std::make_shared<MultiRoom>(this, session, data);
				std::shared_ptr<TetrisRoom> expected = nullptr;
				if (std::atomic_compare_exchange_strong(&rooms[i], &expected, new_room)) {
					active_rooms.AddRoom(data.room_gen, i);
					new_room->SendCreateRoom(session);
					return;
				}
			}
		}
	}

	// 나중에 방 못찾으면 추후 처리 필요
}

void IOCPServer::CreateLockRoom(char* packet, Session& session, int request_gen)
{
	C2S_ADD_LOCK_ROOM_PACKET* lock_p = reinterpret_cast<C2S_ADD_LOCK_ROOM_PACKET*>(packet);
	LockRoomInitData data;
	//std::shared_ptr<TetrisRoom> new_room;
	bool is_single;
	if (lock_p->max_user == 1) {
		data.max_user = lock_p->max_user;
		is_single = true;
	}

	else if (lock_p->max_user == 2 || lock_p->max_user == 5) {
		data.max_user = lock_p->max_user;
		is_single = false;
	}

	else return;
	memcpy(data.room_name, lock_p->room_name, sizeof(data.room_name));
	memcpy(data.room_password, lock_p->room_password, sizeof(data.room_password));
	data.room_gen = GetNewRoomGen();

	std::shared_ptr<TetrisRoom> new_room;

	for (int i = 0; i < MAX_ROOM; ++i) {
		if (rooms[i].load() == nullptr) {
			data.room_index = i;
			{
				std::lock_guard<std::mutex> lock(session.GetMutex());
				if (session.GetState() == SESS_STATE::NONE) return;
				if (session.GetSessionKey().gen != request_gen) return;

				if (is_single) new_room = std::make_shared<SingleRoom>(this, session, data);
				else new_room = std::make_shared<MultiRoom>(this, session, data);
				std::shared_ptr<TetrisRoom> expected = nullptr;
				if (std::atomic_compare_exchange_strong(&rooms[i], &expected, new_room)) {
					active_rooms.AddRoom(data.room_gen, i);
					new_room->SendCreateRoom(session);
					return;
				}
			}
		}
	}
}

void IOCPServer::DeleteRoom(int room_index)
{
	auto room = rooms[room_index].load();
	if (room) {
		active_rooms.RemoveRoom(room->GetRoomGen(), room_index);
	}
	rooms[room_index].store(nullptr);
	std::cout << "Room deleted, Room index: " << room_index << std::endl;
}

void IOCPServer::RequestLoadRanking()
{
	Database& repr_db = GetDB();
	auto task = [&repr_db]() {
		repr_db.ExecuteLoadRanking();
		};
	repr_db.Enqueue(task);
}

int IOCPServer::GetNewUserGen()
{
	return user_gen_generator.fetch_add(1) + 1; // fetch_add는 값을 실제로 원자적으로 증가시키지만, 반환하는 것은 증가 이전의 값
}

int IOCPServer::GetNewRoomGen()
{
	return room_gen_generator.fetch_add(1) + 1;
}

int IOCPServer::GetEmptyUserIndex()
{
	for (int i = 0; i < MAX_USER; ++i) {
		if (users[i].GetState() == SESS_STATE::NONE) {
			if (users[i].TryChangeState(SESS_STATE::NONE, SESS_STATE::LOGIN)) {
				return i;
			}
		}
	}

	return -1;
}

int IOCPServer::GetEmptyRoomIndex()
{
	for (int i = 0; i < MAX_ROOM; ++i) {
		auto room = rooms[i].load();
		if (!room) return i; // 외부에서 받은 인덱스로 cas를 시도함.
	}

	return -1;
}

int IOCPServer::FindSessionIndexById(int user_id)
{
	// 락 안쓴다. 어차피 반환된 세션 락 걸고 또 검증해야 한다.
	return active_users.FindSessionIndexById(user_id);
}

void IOCPServer::Disconnect(int user_index)
{
	Session& target = users[user_index];
	if (target.GetState() == SESS_STATE::NONE) return; // 이미 끊김->또 send -> send 실패 -> PQCS -> Disconnect 무한루프 방지

	if (target.TryChangeDisconnectFlag(true, false)) {
		int user_id = target.GetDBInfo().id;
		if (target.GetState() == SESS_STATE::ROOM) {
			auto room = rooms[target.GetRoomIndex()].load();
			if (room) room->DeleteUser(user_id);
		}

		active_users.RemoveUser(user_id, user_index);
		std::cout << "Session index[" << target.GetSessionKey().index << "] disconnect/Gen: " << target.GetSessionKey().gen << " nickname: " << target.GetDBInfo().nickname << std::endl;
		target.ClearSession();
	}
	
	//S2C_DISCONNECT_PACKET p;
	//p.size = sizeof(S2C_DISCONNECT_PACKET);
	//p.type = S2C_DISCONNECT;
	//SendToSelf(reinterpret_cast<char*>(&p), user_index);

}

void IOCPServer::ProcessDBResult(DBOverlapped* db_over, Session& session, int request_gen)
{
	// DB 작업 이후 결과를 세션에 통지하기 전에, 세션이 종료되었을 수 있다.
	// 만약 세션이 즉시 재사용된다면, 우연히 세션을 초기화하는 과정에서 아이디가 바뀌기 전에 다른 부분이 먼저 변경되었을 가능성이 있다.
	// DB 작업만 문제가 되는게 아니다. 일반 IO도 오퍼레이션 아이디를 넣기는 하지만, 위와 같이 다른 세션인데 우연히 아이디는 바뀌지 않았을 가능성이 있다.
	// 따라서 세션의 초기화는 뮤텍스로 처리해야 한다. 클리어는 굳이 뮤텍스로 처리하지 않아도 될 것 같다. 세션이 비었다는 상태를 클리어 맨 마지막에 저장하면 즉시 재사용된다고 해도 문제는 생기지 않는다.
	
	// IOCP에서 작업 완료하고 얻어온 key만 계속 넘어가면 된다. 최종 검증은 send 직전에 한다.
	// 만약 재사용됐다? -> GQCS에서 받아온 키가 send 전까지 계속 넘어가므로, 최종 검증은 거기서만 하면 된다.
	switch (db_over->type) {
	case DBOperationType::LOGIN: {
		S2C_LOGIN_PACKET login_p;
		S2C_ERROR_PACKET error_p;
		login_p.size = sizeof(S2C_LOGIN_PACKET);
		login_p.type = S2C_LOGIN;
		if (db_over->ok) {
			if (db_over->result_data) { // nullptr이 아니면, 즉 포인터가 존재하면
				if (CheckDuplicateLoginId(static_cast<DBResultLogin*>(db_over->result_data.get())->login_id)) login_p.id = -2;
				else {
					std::lock_guard<std::mutex> lock(session.GetMutex());
					if (request_gen != session.GetSessionKey().gen) return;
					if (session.GetState() == SESS_STATE::NONE) return;

					session.InitDBInfo(static_cast<DBResultLogin*>(db_over->result_data.get()));
					session.StoreState(SESS_STATE::LOBBY);
					active_users.AddUser(session.GetDBInfo().id, session.GetSessionKey().index);
					login_p.id = session.GetDBInfo().id;
					login_p.max_score = session.GetDBInfo().max_score;
					login_p.win_count = session.GetDBInfo().win_count;
					login_p.lose_count = session.GetDBInfo().lose_count;
					StringToCharBuf(session.GetDBInfo().nickname, login_p.nickname, sizeof(login_p.nickname));
				}
			}
			else {
				login_p.id = -1;
			}
		}
		else {
			login_p.id = -1;
		}

		if (login_p.id == -1) {
			error_p.size = sizeof(S2C_ERROR_PACKET);
			error_p.type = S2C_ERROR;
			error_p.error_code = ERROR_CODE::LOGIN_FAILED;
			session.SendPacket(request_gen, reinterpret_cast<char*>(&error_p), iocp_handle);
		}

		else if (login_p.id == -2) {
			error_p.size = sizeof(S2C_ERROR_PACKET);
			error_p.type = S2C_ERROR;
			error_p.error_code = ERROR_CODE::DUPLICATE_LOGIN_ID;
			session.SendPacket(request_gen, reinterpret_cast<char*>(&error_p), iocp_handle);
		}

		else {
			session.SendPacket(request_gen, reinterpret_cast<char*>(&login_p), iocp_handle);

			Database& repr_db = GetDB();
			std::lock_guard<std::mutex> lock(session.GetMutex());
			if (session.GetState() == SESS_STATE::NONE) break; // 리턴하면 db_over 해제가 안됨
			if (session.GetSessionKey().gen != request_gen) break;
			SessionKey key = session.GetSessionKey();
			int user_id = session.GetDBInfo().id;
			auto task = [&repr_db, key, user_id]() {
				repr_db.ExecuteLoadFriendList(key, user_id);
				};
			repr_db.Enqueue(task);
		}

		break;
	}

	case DBOperationType::UPDATE_SCORE:
		if (db_over->ok) {
			// void*는 사용할 때 타입을 명시해야함(컴파일러가 알아들을 수 있도록)
			// 보이드 유니크 포인터인 db_over->info를 get 함수로 raw 포인터를 가져와 사용할 포인터로 static_cast, void* <-> T* 간에는 static_cast가 허용되고, void*에는 T*를 대입할 수 있다.
			DBResultUpdateScore* res = static_cast<DBResultUpdateScore*>(db_over->result_data.get());
			{
				std::lock_guard<std::mutex> lock(session.GetMutex());
				if (request_gen != session.GetSessionKey().gen) return;
				if (session.GetState() == SESS_STATE::NONE) return;
				session.GetDBInfo().max_score = res->max_score;
				ranking_manager.UpdateRanking(session.GetDBInfo().id, session.GetDBInfo().nickname, res->max_score);
			}
			S2C_UPDATE_SCORE_PACKET us_p;
			us_p.size = sizeof(S2C_UPDATE_SCORE_PACKET);
			us_p.type = S2C_UPDATE_SCORE;
			us_p.max_score = res->max_score;
			session.SendPacket(request_gen, reinterpret_cast<char*>(&us_p), iocp_handle);
		}
		break;

	case DBOperationType::UPDATE_MATCH_RESULT:
		if (db_over->ok) {
			S2C_MATCH_RECORD_PACKET record_p;
			record_p.size = sizeof(S2C_MATCH_RECORD_PACKET);
			record_p.type = S2C_MATCH_RECORD;
			DBResultUpdateMatchResult* res = static_cast<DBResultUpdateMatchResult*>(db_over->result_data.get());
			{
				std::lock_guard<std::mutex> lock(session.GetMutex());
				if (request_gen != session.GetSessionKey().gen) break;
				if (session.GetState() == SESS_STATE::NONE) break;

				if (res->is_winner) ++session.GetDBInfo().win_count;
				else ++session.GetDBInfo().lose_count;

				record_p.win_count = session.GetDBInfo().win_count;
				record_p.lose_count = session.GetDBInfo().lose_count;
			}

			session.SendPacket(request_gen, reinterpret_cast<char*>(&record_p), iocp_handle);
		}
		break;

	case DBOperationType::ADD_FRIEND_REQUEST: {
		// 요청한 사람이 받는 IO
		if (db_over->ok) {
			DBResultAddFriendRequest* res = static_cast<DBResultAddFriendRequest*>(db_over->result_data.get());
			int recver_index = FindSessionIndexById(res->recver_info.id);
			if (recver_index != -1) { // 요청받는 사람은 실제 본인이어야 함
				Session& recver_session = users[recver_index];
				std::lock_guard<std::mutex> lock(recver_session.GetMutex());
				if (recver_session.GetState() != SESS_STATE::LOBBY) break;
				if (recver_session.GetDBInfo().id != res->recver_info.id) break;

				S2C_REQUEST_FRIEND_PACKET request_p;
				request_p.size = sizeof(S2C_REQUEST_FRIEND_PACKET);
				request_p.type = S2C_REQUEST_FRIEND;
				request_p.requester_id = res->requester_info.id; // 요청자 정보 넣기
				StringToCharBuf(res->requester_info.nickname, request_p.requester_nickname, MAX_USER_NAME);
				recver_session.SendPacket(reinterpret_cast<char*>(&request_p), iocp_handle);
			}
		}
		break;
	}

	case DBOperationType::ADD_FRIEND:
		if (db_over->ok) {
			DBResultAddFriend* res = static_cast<DBResultAddFriend*>(db_over->result_data.get());
			SendAddFriendResult(res->requester_info, res->accepter_info);
		}
		break;

	case DBOperationType::DELETE_FRIEND:
		if (db_over->ok) {
			DBResultDeleteFriend* res = static_cast<DBResultDeleteFriend*>(db_over->result_data.get());
			SendDeleteFriendResult(res->requester_id, res->target_id);
		}
		break;
		
	case DBOperationType::LOAD_FRIEND_LIST: // 얘는 검증하면 됨
		if (db_over->ok) {
			DBResultLoadFriendList* res = static_cast<DBResultLoadFriendList*>(db_over->result_data.get());
			std::lock_guard<std::mutex> lock(session.GetMutex());
			if (session.GetState() == SESS_STATE::NONE) break;
			if (request_gen != session.GetSessionKey().gen) break;

			// 클라는 상태가 변경되어야 친구를 볼 수 있어서(상태에 따라 받을 수 있는 패킷이 다르다) 상태 변경 시 따로 리스트를 요청하게 되어 있다
			session.InitFriendList(res->friend_list); 
		}
		break;
	}

	delete db_over;
}

void IOCPServer::ProcessRankingResult(DBOverlapped* db_over)
{
	if (db_over->ok && db_over->result_data) {
		DBResultLoadRanking* res = static_cast<DBResultLoadRanking*>(db_over->result_data.get());
		ranking_manager.LoadInitialRanking(res->rankings);
	}

	delete db_over;
}

void IOCPServer::StringToCharBuf(const std::string& str, char* buf, int buf_size)
{
	ZeroMemory(buf, buf_size);
	int copy_size = std::min(str.size(), static_cast<size_t>(buf_size));
	memcpy(buf, str.data(), copy_size);
}

std::string IOCPServer::CharBufToString(const char* buf, int buf_size)
{
	int copy_size = strnlen(buf, buf_size);
	std::string str(buf, copy_size);
	return str;
}

