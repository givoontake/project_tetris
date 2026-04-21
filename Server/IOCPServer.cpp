#include <iostream>
#include <algorithm>
#include "IOCPServer.h"
#include "SingleRoom.h"
#include "MultiRoom.h"

#undef min

IOCPServer::IOCPServer() : packet_handler(*this), db_result_handler(*this)
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


void IOCPServer::SendRoomList(Session& session, int request_gen)
{
	// 더미 방 생성
	//for (int i = 0; i < 20; ++i) {
	//	S2C_ROOM_INFO_PACKET p{};
	//	p.size = sizeof(S2C_ROOM_INFO_PACKET);
	//	p.type = S2C_ROOM_INFO;

	//	p.room_gen = 1,000,000 + i;

	//	std::string name = "DummyRoom_" + std::to_string(i);
	//	StringToCharBuf(name, p.room_name, sizeof(p.room_name));

	//	p.max_user = 2;
	//	p.cur_user = 1; 

	//	p.is_private = false;
	//	p.is_play = false;

	//	session.SendPacket(request_gen, reinterpret_cast<char*>(&p), iocp_handle);
	//}

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
		info_p.header.size = static_cast<std::uint16_t>(sizeof(info_p));
		info_p.header.type = S2C_ROOM_INFO;
		info_p.room_gen = room_sp->GetRoomGen();
		const std::string& name = room_sp->GetRoomName();
		info_p.max_user = room_sp->GetMaxUser();
		info_p.cur_user = room_sp->GetCurrentUser();
		StringToCharBuf(name, info_p.room_name, sizeof(info_p.room_name));
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
bool IOCPServer::TryJoinRoom(Session& session, int request_gen, int room_gen, const std::string& room_password)
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
	error_p.header.size = static_cast<std::uint16_t>(sizeof(error_p));
	error_p.header.type = S2C_ERROR;
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
				if (room_sp->GetIsPrivate()) continue;
				if (room_sp->GetMaxUser() == 2 or room_sp->GetMaxUser() == 5) { // 공개 멀티 방 중 아무 방이나 찾기
					if (TryJoinRoom(session, request_gen, room_sp->GetRoomGen(), "")) return;
				}
			}
		}
	}

	else if (max_user == 2) {
		for (auto& room : rooms) {
			auto room_sp = room.load();
			if (room_sp) {
				if (room_sp->GetIsPrivate()) continue;
				if (room_sp->GetMaxUser() == max_user) {
					if (TryJoinRoom(session, request_gen, room_sp->GetRoomGen(), "")) return;
				}
			}
		}
	}

	else if (max_user == 5) {
		for (auto& room : rooms) {
			auto room_sp = room.load();
			if (room_sp) {
				if (room_sp->GetIsPrivate()) continue;
				if (room_sp->GetMaxUser() == max_user) {
					if (TryJoinRoom(session, request_gen, room_sp->GetRoomGen(), "")) return;
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
		info_p.header.size = static_cast<std::uint16_t>(sizeof(info_p));
		info_p.header.type = S2C_LOBBY_USER_INFO;
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
		info_p.header.size = static_cast<std::uint16_t>(sizeof(info_p));
		info_p.header.type = S2C_FRIEND_INFO;
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
		info_p.header.size = static_cast<std::uint16_t>(sizeof(info_p));
		info_p.header.type = S2C_RANKING_INFO;
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
	add_p.header.size = static_cast<std::uint16_t>(sizeof(add_p));
	add_p.header.type = S2C_ADD_FRIEND;

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
	delete_p.header.size = static_cast<std::uint16_t>(sizeof(delete_p));
	delete_p.header.type = S2C_DELETE_FRIEND;

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
	CreateIoCompletionPort(reinterpret_cast<HANDLE>(listen_socket), iocp_handle, LISTEN_IO_COMPLETION, 0);
	int addr_size = sizeof(SOCKADDR_IN);
	AcceptEx(listen_socket, client_socket, accept_over.packet_buf, 0, addr_size + 16, addr_size + 16, 0, &accept_over.ex_over.over);
}

void IOCPServer::ProcessGQCS()
{
	while (is_running) {
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
		switch (completion_key) {
		case LISTEN_IO_COMPLETION: {
			if (ex_over->op_type != OP_TYPE::ACCEPT) break;
			if (!result) {
				std::cout << "Accept Error" << WSAGetLastError() << "\n";
				break;
			}
			int new_index = GetEmptyUserIndex();
			if (new_index != -1) {
				int gen = GetNewUserGen();
				users[new_index].InitSession(gen, client_socket);
				CreateIoCompletionPort(reinterpret_cast<HANDLE>(client_socket), iocp_handle, SESSION_IO_COMPLETION, 0);
				users[new_index].RecvPacket(users[new_index].GetSessionKey().gen, iocp_handle);
				client_socket = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);
				std::cout << "Session[" << new_index << "] connect/Gen: " << users[new_index].GetSessionKey().gen << std::endl;
			}

			else {
				closesocket(client_socket);
				client_socket = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);
			}

			if (client_socket != INVALID_SOCKET) {
				ZeroMemory(&accept_over.ex_over.over, sizeof(accept_over.ex_over.over));
				int addr_size = sizeof(SOCKADDR_IN);
				bool res = AcceptEx(listen_socket, client_socket, accept_over.packet_buf, 0, addr_size + 16, addr_size + 16, 0, &accept_over.ex_over.over);
				if (!res && WSAGetLastError() != ERROR_IO_PENDING) std::cerr << "AcceptEx fail.. " << std::endl;
			}
			break;
		}

		case SESSION_IO_COMPLETION: {
			IOOverlapped* io_over = reinterpret_cast<IOOverlapped*>(ex_over);
			Session& sess = users[ex_over->key.index];
			switch (ex_over->op_type) {
			case OP_TYPE::RECV: {
				if (!result || transferred_bytes == 0) {
					sess.StoreDisconnectFlag(true);
					Disconnect(sess.GetSessionKey());
					break;
				}
				ProcessPacket(sess, io_over->ex_over.key.gen, transferred_bytes);
				sess.RecvPacket(io_over->ex_over.key.gen, iocp_handle);
				break;
			}

			case OP_TYPE::SEND: {
				if ((!result || transferred_bytes == 0)) {
					sess.StoreDisconnectFlag(true);
					Disconnect(sess.GetSessionKey());
				}
				delete io_over;
				break;
			}

			case OP_TYPE::DISCONNECT:
				Disconnect(ex_over->key);
				delete ex_over;
				break;

			default:
				break;
			}
			break;
		}

		case ROOM_IO_COMPLETION:
			if (ex_over->op_type == OP_TYPE::DELETE_ROOM) DeleteRoom(ex_over->room_index);
			delete ex_over;
			break;

		case DB_IO_COMPLETION: {
			DBOverlapped* db_over = reinterpret_cast<DBOverlapped*>(ex_over);
			if (!result) {
				delete db_over;
				break;
			}
			int index = db_over->ex_over.key.index;
			if (index > -1) {
				Session& sess = users[db_over->ex_over.key.index];
				db_result_handler.HandleDBResult(db_over, sess);
			}
			else db_result_handler.HandleDBResult(db_over);

			delete db_over;
			break;
		}

		default:
			break;
		}
	}
}
void IOCPServer::ProcessPacket(Session& session, int request_gen, int recv_bytes)
{   
	std::uint16_t packet_size = 0;
	int offset = 0;
	char p_buffer[BUF_SIZE];
	int remain_data_size = 0;
	bool disconnect_flag = false;

	// 작업에 사용해야 할 세션 내의 값들을 세션 락을 걸고 안전하게 복사해온다.
	// 복사한 값을 그 다음에 처리하는 것은 문제 없다. 처리 도중 재사용된다고 해도 어차피 같은 세션인지 계속 검증하므로 걸러진다
	// 작업을 완료하면 세션에 반영되어야 하는 remain_data_size만 락을 걸고 세팅한다.
	{
		std::lock_guard<std::mutex> lock(session.GetMutex());

		if (session.GetState() == SESS_STATE::NONE) return;
		if (session.GetSessionKey().gen != request_gen) return;

		if (recv_bytes + session.GetRemainDataSize() > BUF_SIZE) {
			session.StoreDisconnectFlag(true);
			ExOverlapped* disconnect_over = new ExOverlapped;
			disconnect_over->op_type = OP_TYPE::DISCONNECT;
			disconnect_over->key = session.GetSessionKey();
			PostQueuedCompletionStatus(iocp_handle, 0, SESSION_IO_COMPLETION, reinterpret_cast<WSAOVERLAPPED*>(disconnect_over));
			return;
		}
		
		else session.AddDataSize(recv_bytes);

		if (session.GetRemainDataSize() < PACKET_HEADER_SIZE) return;

		remain_data_size = session.GetRemainDataSize();
		packet_size = reinterpret_cast<PacketHeader*>(session.GetExOver().packet_buf)->size;
		memcpy(p_buffer, session.GetExOver().packet_buf, remain_data_size);
	}

	if (packet_size < PACKET_HEADER_SIZE || packet_size > BUF_SIZE) {
		session.StoreDisconnectFlag(true);
		ExOverlapped* disconnect_over = new ExOverlapped;
		disconnect_over->op_type = OP_TYPE::DISCONNECT;
		disconnect_over->key = session.GetSessionKey();
		PostQueuedCompletionStatus(iocp_handle, 0, SESSION_IO_COMPLETION, reinterpret_cast<WSAOVERLAPPED*>(disconnect_over));
		return;
	}

	while (remain_data_size - offset >= packet_size)
	{
		char* packet = p_buffer + offset;
		RoutePacket(packet, session, request_gen);

		offset += packet_size;
		if (remain_data_size - offset < PACKET_HEADER_SIZE) break;
		packet_size = reinterpret_cast<PacketHeader*>(p_buffer + offset)->size;
		if (packet_size < PACKET_HEADER_SIZE || packet_size > BUF_SIZE) {
			session.StoreDisconnectFlag(true);
			ExOverlapped* disconnect_over = new ExOverlapped;
			disconnect_over->op_type = OP_TYPE::DISCONNECT;
			disconnect_over->key = session.GetSessionKey();
			PostQueuedCompletionStatus(iocp_handle, 0, SESSION_IO_COMPLETION, reinterpret_cast<WSAOVERLAPPED*>(disconnect_over));
			return;
		}
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
	PrintPacketType(reinterpret_cast<PacketHeader*>(packet)->type);
	switch (session.GetState()) {
	case SESS_STATE::NONE:
		return;
	case SESS_STATE::LOGIN:
		packet_handler.HandlePacket(packet, session, request_gen);
		break;
	case SESS_STATE::LOBBY:
		packet_handler.HandlePacket(packet, session, request_gen);
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
	constexpr size_t MIN_ROOM_NAME_LENGTH = 4;
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
	data.room_name = CharBufToString(open_p->room_name, sizeof(open_p->room_name));
	if (data.room_name.size() < MIN_ROOM_NAME_LENGTH) {
		SendError(session, request_gen, ERROR_CODE::INVALID_REQUEST);
		return;
	}
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
	constexpr size_t MIN_ROOM_NAME_LENGTH = 4;
	constexpr size_t MIN_ROOM_PASSWORD_LENGTH = 4;
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
	data.room_name = CharBufToString(lock_p->room_name, sizeof(lock_p->room_name));
	data.room_password = CharBufToString(lock_p->room_password, sizeof(lock_p->room_password));
	if (data.room_name.size() < MIN_ROOM_NAME_LENGTH || data.room_password.size() < MIN_ROOM_PASSWORD_LENGTH) {
		SendError(session, request_gen, ERROR_CODE::INVALID_REQUEST);
		return;
	}
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

void IOCPServer::Disconnect(SessionKey session_key) // 생각해보니 GEN이 없으면 무조건 DISCONNECT라서 조금 문제가 있을 것 같은데?? + 중간상태 검토가 필요할 것 같다..
{
	const int user_index = session_key.index;
	Session& target = users[user_index];
	if (target.GetState() == SESS_STATE::NONE) return; // 이미 끊김->또 send -> send 실패 -> PQCS -> Disconnect 무한루프 방지
	if (target.GetSessionKey().gen != session_key.gen) return;

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

void IOCPServer::StringToCharBuf(const std::string& str, char* buf, int buf_size)
{
	ZeroMemory(buf, buf_size);
	size_t copy_size = std::min(str.size(), static_cast<size_t>(buf_size));
	memcpy(buf, str.data(), copy_size);
}

std::string IOCPServer::CharBufToString(const char* buf, int buf_size)
{
	size_t copy_size = strnlen(buf, static_cast<size_t>(buf_size));
	std::string str(buf, copy_size);
	return str;
}

