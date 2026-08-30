#include <iostream>
#include <algorithm>
#include "IOCPServer.h"
#include "SingleRoom.h"
#include "MultiRoom.h"

#undef min

IOCPServer::IOCPServer()
	: packet_handler(*this), db_result_handler(*this)
{
	for (int i = 0; i < MAX_USER; ++i) {
		users[i].store(nullptr);
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
		std::atomic_store(&room, SP<TetrisRoom>{}); // nullptr과 같은 논리
	}
	active_rooms.Clear();
	active_users.Clear();
	closesocket(listen_socket);
	closesocket(client_socket);
	StopDBWorkers();
	WSACleanup();
}

void IOCPServer::InitDBWorkers()
{
	login_db_worker.Init(iocp_handle);
	for (auto& worker : game_db_workers)
		worker.Init(iocp_handle);
}

void IOCPServer::StartDBWorkers()
{
	login_db_worker.Start();
	for (auto& worker : game_db_workers)
		worker.Start();
}

void IOCPServer::StopDBWorkers()
{
	login_db_worker.Stop();
	for (auto& worker : game_db_workers)
		worker.Stop();
}

void IOCPServer::WakeDBWorkers()
{
	login_db_worker.Wake();
	for (auto& worker : game_db_workers)
		worker.Wake();
}

void IOCPServer::RunLoginDBWorker()
{
	login_db_worker.Run();
}

void IOCPServer::RunGameDBWorker(std::size_t worker_index)
{
	if (worker_index >= game_db_workers.size()) return;
	game_db_workers[worker_index].Run();
}

bool IOCPServer::EnqueueDBTask(std::unique_ptr<ServerDBTask> db_task)
{
	const std::size_t worker_index = next_game_db_worker.fetch_add(1) % game_db_workers.size();
	return game_db_workers[worker_index].Enqueue(std::move(db_task));
}

bool IOCPServer::EnqueueDBTask(std::unique_ptr<SessionDBTask> db_task, const SP<Session>& session)
{
	bool enqueued = false;
	if (db_task->type == DBOperationType::LOGIN)
	{
		enqueued = login_db_worker.Enqueue(std::move(db_task), session);
	}
	else
	{
		std::size_t worker_index = 0;
		if (db_task->key.id >= 0)
			worker_index = static_cast<std::size_t>(db_task->key.id) % game_db_workers.size();
		else
			worker_index = next_game_db_worker.fetch_add(1) % game_db_workers.size();

		enqueued = game_db_workers[worker_index].Enqueue(std::move(db_task), session);
	}

	if (!enqueued && session->TryDeactivate()) TryDisconnect(session);
	return enqueued;
}

void IOCPServer::EnqueueDBTask(std::unique_ptr<MultiSessionDBTask> db_task, const SP<Session> (&sessions)[MAX_MATCH_RESULT_PLAYERS])
{
	for (int i = 0; i < db_task->player_count; ++i)
	{
		if (sessions[i]->TryAddPending()) db_task->completion_mask |= static_cast<uint8_t>(1u << i);
		else if (sessions[i]->TryDeactivate()) TryDisconnect(sessions[i]);
	}

	const std::size_t worker_index = static_cast<std::size_t>(db_task->players[0].id) % game_db_workers.size();
	game_db_workers[worker_index].Enqueue(std::move(db_task));
}


void IOCPServer::SendRoomList(const SP<Session>& session)
{
	if (!session) return;
	// 더미 방 생성
	for (int i = 0; i < 20; ++i) {
		S2C_ROOM_INFO_PACKET p{};
		p.header.size = sizeof(S2C_ROOM_INFO_PACKET);
		p.header.type = S2C_ROOM_INFO;

		p.room_gen = 1000000 + i;

		std::string name = "DummyRoom_" + std::to_string(i);
		StringToCharBuf(name, p.room_name, sizeof(p.room_name));

		p.max_user = 2;
		p.cur_user = 1; 

		p.is_private = false;
		p.is_play = false;

		session->SendPacket(reinterpret_cast<char*>(&p), iocp_handle);
	}
	
	char packet_buf[BUF_SIZE];
	int packet_size = 0;
	for (auto& room_sp : active_rooms.GetActiveRoomsSnapshot()) {
		if (!room_sp) continue;
		S2C_ROOM_INFO_PACKET info_p;
		RoomInfoSnapshot room_snapshot = room_sp->GetRoomInfoSnapshot();
		if (room_snapshot.room_state == ROOM_STATE::EMPTY) continue;
		info_p.header.size = static_cast<std::uint16_t>(sizeof(info_p));
		info_p.header.type = S2C_ROOM_INFO;
		info_p.room_gen = room_snapshot.room_gen;
		info_p.max_user = room_snapshot.max_user;
		info_p.cur_user = room_snapshot.cur_user;
		StringToCharBuf(room_snapshot.room_name, info_p.room_name, sizeof(info_p.room_name));
		info_p.is_private = room_snapshot.is_private;
		bool is_play;
		if (room_snapshot.room_state == ROOM_STATE::WAIT) is_play = false;
		else is_play = true;
		info_p.is_play = is_play;

		if (packet_size + sizeof(info_p) > BUF_SIZE) { 
			session->SendBoundPacket(reinterpret_cast<char*>(packet_buf), packet_size, iocp_handle);
			packet_size = 0;
		}

		memcpy(packet_buf + packet_size, &info_p, sizeof(info_p));
		packet_size += sizeof(info_p);
		//std::lock_guard<std::mutex> lock(session->GetMutex()); // 조건 불일치시 바로 리턴을 위해 체크만 하자, sendpacket()내에 뮤텍스 있어서 넣으면 데드락
		//if (session->GetSessionKey().gen != request_gen) return;
		//if (session->GetState() == SESS_STATE::NONE) return;
	}
	session->SendBoundPacket(reinterpret_cast<char*>(packet_buf), packet_size, iocp_handle);
}

// 
int IOCPServer::TryJoinRoom(const SP<Session>& session, int room_gen, const std::string& room_password)
{	
	if (!session) return ERROR_CODE::INVALID_REQUEST;
	int result = ERROR_CODE::ROOM_NOT_FOUND;
	auto session_ptr = FindSessionByIndex(session->GetSessionKey().index);
	if (!session_ptr || session_ptr != session) {
		return ERROR_CODE::INVALID_REQUEST;
	}
	SP<TetrisRoom> room_sp = FindRoom(room_gen);
	if (room_sp) {
		std::lock_guard<std::mutex> lock(room_sp->GetRoomMutex());
		if (room_sp->GetRoomState() == ROOM_STATE::WAITING_DELETE) result = ERROR_CODE::ROOM_NOT_FOUND;
		else {
			if (room_sp->GetMaxUser() != 1) { // 싱글이 아닌 경우
				auto multi_sp = std::dynamic_pointer_cast<MultiRoom>(room_sp); // TetrisRoom -> MultiRoom으로 다운캐스팅(참조 카운트 증가)
				if (!multi_sp) result = ERROR_CODE::SERVER_ERROR;
				else {
					result = multi_sp->AddUser(session_ptr, room_password); // 멀티 룸에만 있는 함수라 위에서 다운캐스팅 한 것
				}
			}
			else {
				result = ERROR_CODE::INVALID_REQUEST;
			}
		}
	}

	return result;
}

SP<TetrisRoom> IOCPServer::FindRoom(int room_gen)
{
	return active_rooms.FindRoom(room_gen);
}

int IOCPServer::FindUser(int user_id)
{
	for (int i = 0; i < users.size(); ++i) { // 다른 세션 찾는데 락을 걸어버리면 좀 이상하다. 
		auto user = users[i].load();
		if (user && user->GetDBInfo().id == user_id) return i;
	}

	return -1;
}

void IOCPServer::SendError(const SP<Session>& session, int error_code)
{
	if (!session) return;
	S2C_ERROR_PACKET error_p;
	error_p.header.size = static_cast<std::uint16_t>(sizeof(error_p));
	error_p.header.type = S2C_ERROR;
	error_p.error_code = error_code;

	session->SendPacket(reinterpret_cast<char*>(&error_p), iocp_handle);
}

void IOCPServer::FindMatch(const SP<Session>& session, int max_user)
{
	if (!session) return;
	if (max_user == 0) {
		for (auto& room : rooms) {
			// 방에 접근할 때는 무조건 Shared_ptr을 로드해서 참조 카운트를 늘려야 한다. 방이 삭제되더라도 안전하게 동작하기 위해서이다.
			// 단순히 널을 체크하고 들어가도 그 다음 내부 객체 접근 시 그 객체가 삭제되었을 수 있다.
			auto room_sp = room.load();
			if (room_sp) {
				if (room_sp->GetIsPrivate()) continue;
				if (room_sp->GetMaxUser() == 2 or room_sp->GetMaxUser() == 5) { // 공개 멀티 방 중 아무 방이나 찾기
					int result = TryJoinRoom(session, room_sp->GetRoomGen(), "");
					if (result == SUCCESS) return;
					if (result == ERROR_CODE::INVALID_REQUEST || result == ERROR_CODE::SERVER_ERROR) {
						SendError(session, result);
						return;
					}
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
					int result = TryJoinRoom(session, room_sp->GetRoomGen(), "");
					if (result == SUCCESS) return;
					if (result == ERROR_CODE::INVALID_REQUEST || result == ERROR_CODE::SERVER_ERROR) {
						SendError(session, result);
						return;
					}
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
					int result = TryJoinRoom(session, room_sp->GetRoomGen(), "");
					if (result == SUCCESS) return;
					if (result == ERROR_CODE::INVALID_REQUEST || result == ERROR_CODE::SERVER_ERROR) {
						SendError(session, result);
						return;
					}
				}
			}
		}
	}

	else {
		SendError(session, ERROR_CODE::INVALID_REQUEST);
		return;
	}
	
	SendError(session, ERROR_CODE::NOT_FOUND_JOINABLE_ROOM);
}

void IOCPServer::SendLobbyUserList(const SP<Session>& session)
{
	if (!session) return;
	{
		// 비용을 줄이기 위한 선체크
		if (session->GetModeState() != MODE_STATE::LOBBY) return;
	}
	
	int packet_size = 0;
	char packet_buf[BUF_SIZE];
	for (auto& user : active_users.GetActiveSessions()) {
		S2C_LOBBY_USER_INFO_PACKET info_p;
		info_p.header.size = static_cast<std::uint16_t>(sizeof(info_p));
		info_p.header.type = S2C_LOBBY_USER_INFO;
		//info_p.user_id = -1;
		{
			if (user->GetDBInfo().id == session->GetDBInfo().id) continue;
			if (user->GetModeState() == MODE_STATE::LOBBY) {
				info_p.user_id = user->GetDBInfo().id;
				StringToCharBuf(user->GetDBInfo().nickname, info_p.nickname, MAX_ROOM_NAME);
			}
			else continue;
		}
		
		if (packet_size + sizeof(info_p) > BUF_SIZE) {
			session->SendBoundPacket(reinterpret_cast<char*>(packet_buf), packet_size, iocp_handle);
			packet_size = 0;
		}

		memcpy(packet_buf + packet_size, &info_p, sizeof(info_p));
		packet_size += sizeof(info_p);
	}
	session->SendBoundPacket(reinterpret_cast<char*>(packet_buf), packet_size, iocp_handle);
}

void IOCPServer::SendFriendList(const SP<Session>& session)
{
	if (!session) return;
	std::vector<FriendInfo> friend_list;
	{
		if (session->GetModeState() != MODE_STATE::LOBBY) return;
		friend_list = session->GetFriendList();
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
		auto sess = active_users.FindSessionById(friend_info.id);
		if (sess) {
			if (sess->GetDBInfo().id != friend_info.id) continue;
			if (sess->GetModeState() == MODE_STATE::LOBBY) info_p.is_lobby = true;
		}

		if (packet_size + sizeof(info_p) > BUF_SIZE) {
			session->SendBoundPacket(reinterpret_cast<char*>(packet_buf), packet_size, iocp_handle);
			packet_size = 0;
		}

		memcpy(packet_buf + packet_size, &info_p, sizeof(info_p));
		packet_size += sizeof(info_p);
	}
	session->SendBoundPacket(reinterpret_cast<char*>(packet_buf), packet_size, iocp_handle);
}

void IOCPServer::SendRanking(const SP<Session>& session)
{
	if (!session) return;
	{
		if (session->GetModeState() != MODE_STATE::LOBBY) return;
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
			session->SendBoundPacket(reinterpret_cast<char*>(packet_buf), packet_size, iocp_handle);
			packet_size = 0;
		}

		memcpy(packet_buf + packet_size, &info_p, sizeof(info_p));
		packet_size += sizeof(info_p);
	}

	session->SendBoundPacket(reinterpret_cast<char*>(packet_buf), packet_size, iocp_handle);
}

void IOCPServer::SendAddFriendResult(FriendInfo& requester_info, FriendInfo& accepter_info)
{
	bool requester_connected = false;
	S2C_ADD_FRIEND_PACKET add_p;
	add_p.header.size = static_cast<std::uint16_t>(sizeof(add_p));
	add_p.header.type = S2C_ADD_FRIEND;

	auto requester_sess = active_users.FindSessionById(requester_info.id);
	if (requester_sess && requester_sess->GetDBInfo().id == requester_info.id) {
		requester_sess->AddFriend(accepter_info);
		if ((requester_sess->GetModeState() == MODE_STATE::LOBBY)) requester_connected = true;
	}

	if (requester_connected) {
		add_p.friend_id = accepter_info.id;
		StringToCharBuf(accepter_info.nickname, add_p.friend_nickname, MAX_USER_NAME);
		requester_sess->SendPacket(reinterpret_cast<char*>(&add_p), iocp_handle);
	}
	
	bool accepter_connected = false;
	auto accepter_sess = active_users.FindSessionById(accepter_info.id);
	if (accepter_sess && accepter_sess->GetDBInfo().id == accepter_info.id) {
		accepter_sess->AddFriend(requester_info);
		if (accepter_sess->GetModeState() == MODE_STATE::LOBBY) accepter_connected = true;
	}

	if (accepter_connected) {
		add_p.friend_id = requester_info.id;
		StringToCharBuf(requester_info.nickname, add_p.friend_nickname, MAX_USER_NAME);
		accepter_sess->SendPacket(reinterpret_cast<char*>(&add_p), iocp_handle);
	}
}

// DB는 변경이 성공한 경우에 작업 성공으로 판정하도록 했으므로 결과를 통지할 세션의 존재 유무만 체크하면 된다.
void IOCPServer::SendDeleteFriendResult(int requester_id, int target_id)
{
	// 세션은 재사용하므로 논리적으로는 포인터는 항상 유효
	bool requester_connected = false;
	S2C_DELETE_FRIEND_PACKET delete_p;
	delete_p.header.size = static_cast<std::uint16_t>(sizeof(delete_p));
	delete_p.header.type = S2C_DELETE_FRIEND;

	auto requester_sess = active_users.FindSessionById(requester_id);
	if (requester_sess && requester_sess->GetDBInfo().id == requester_id) { // 안전성 + 가드
		requester_sess->DeleteFriend(target_id);
		if (requester_sess->GetModeState() == MODE_STATE::LOBBY) requester_connected = true;
	}

	if (requester_connected) {
		delete_p.target_id = target_id;
		requester_sess->SendPacket(reinterpret_cast<char*>(&delete_p), iocp_handle);
	}
	
	bool target_connected = false;
	auto target_sess = active_users.FindSessionById(target_id);
	if (target_sess && target_sess->GetDBInfo().id == target_id) {
		target_sess->DeleteFriend(requester_id);
		if (target_sess->GetModeState() == MODE_STATE::LOBBY)  target_connected = true;
	}

	if (target_connected) {
		delete_p.target_id = requester_id;
		target_sess->SendPacket(reinterpret_cast<char*>(&delete_p), iocp_handle);
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
	while (is_running.load()) {
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
				auto new_session = std::make_shared<Session>();
				new_session->SetIndex(new_index);
				new_session->InitSession(client_socket);
				SP<Session> expected = nullptr;
				if (users[new_index].compare_exchange_strong(expected, new_session)) {
					CreateIoCompletionPort(reinterpret_cast<HANDLE>(client_socket), iocp_handle, SESSION_IO_COMPLETION, 0);
					new_session->RecvPacket(iocp_handle);
					client_socket = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);
					std::cout << "Session[" << new_index << "] connect" << std::endl;
				}
				else {
					closesocket(client_socket);
					client_socket = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);
				}
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
			auto sess_ptr = FindSessionByIndex(ex_over->key.index);
			if (!sess_ptr) {
				if (ex_over->op_type == OP_TYPE::SEND) delete io_over;
				break;
			}
			Session& sess = *sess_ptr;
			switch (ex_over->op_type) {
			case OP_TYPE::RECV: {
				if (!result || transferred_bytes == 0) {
					if (sess.BeginDeactivate()) {
						BeginDisconnect(sess_ptr); // 팬딩이 0으로 노출되면 다른 곳에서 disconnect 관련 작업이 일어날 수 있다.
					}
					sess.ReducePending();
					if (sess.TryDeactivate()) TryDisconnect(sess_ptr);

					break;
				}
				else {
					ProcessPacket(sess_ptr, transferred_bytes); // recv 토큰은 정상 수신 중 유지하고 disconnect 경로에서만 줄인다.
					sess.ReducePending();
					if (sess.TryDeactivate()) TryDisconnect(sess_ptr);
					sess.RecvPacket(iocp_handle);
				}
				break;
			}

			case OP_TYPE::SEND: {
				if ((!result || transferred_bytes == 0)) {
					if (sess.BeginDeactivate()) {
						BeginDisconnect(sess_ptr);
					}
					sess.ReducePending();
					if (sess.TryDeactivate()) TryDisconnect(sess_ptr);
				}
				else {
					sess.ReducePending();
					if (sess.TryDeactivate()) TryDisconnect(sess_ptr);
				}
				
				delete io_over;
				break;
			}

			default:
				break;
			}
			break;
		}

		case ROOM_IO_COMPLETION:
			if (ex_over->op_type == OP_TYPE::DELETE_ROOM) DeleteRoom(ex_over->room_index);
			delete ex_over;
			break;

		case DB_SESSION_COMPLETION: { // 팬딩은 모든 작업을 마치고 줄야야 함
			DBOverlapped* db_over = reinterpret_cast<DBOverlapped*>(ex_over);
			int index = db_over->ex_over.key.index;
			auto sess_ptr = FindSessionByIndex(index);
			if (!sess_ptr) {
				delete db_over;
				break;
			}
			if (!result) {
				if (sess_ptr->BeginDeactivate()) {
					BeginDisconnect(sess_ptr);
				}
			}
			else {
				db_result_handler.HandleIOResult(db_over, sess_ptr);
			}
			sess_ptr->ReducePending();
			if (sess_ptr->TryDeactivate()) TryDisconnect(sess_ptr);
			delete db_over;
			break;
		}

		case DB_SERVER_COMPLETION: {
			if (!result) {
				is_running.store(false);
			}
			DBOverlapped* db_over = reinterpret_cast<DBOverlapped*>(ex_over);
			db_result_handler.HandleInitServerResult(db_over);
			delete db_over;
			break;
		}
		
		default:
			break;
		}
	}
}
void IOCPServer::ProcessPacket(const SP<Session>& session, int recv_bytes)
{   
	if (!session) return;
	std::uint16_t packet_size = 0;
	int offset = 0;
	char p_buffer[BUF_SIZE];
	int remain_data_size = 0;

	if (recv_bytes + session->GetRemainDataSize() > BUF_SIZE) return; // 버퍼가 더 이상 없다면 종료
	else session->AddDataSize(recv_bytes);

	if (session->GetRemainDataSize() < PACKET_HEADER_SIZE) return; // 처리할 최소 데이터(헤더 크기 이상)가 없다면 종료

	// 우선 사이즈 - 타입 관계는 신뢰를 전제로 간다. 보안 처리는 나중에 고민할 예정
	remain_data_size = session->GetRemainDataSize();
	packet_size = reinterpret_cast<PacketHeader*>(session->GetExOver().packet_buf)->size;
	memcpy(p_buffer, session->GetExOver().packet_buf, remain_data_size);

	//if (packet_size < PACKET_HEADER_SIZE || packet_size > BUF_SIZE) { // 이거 반쪽짜리 방어인데?? 있으나~ 없으나~ 어차피 범위를 벗어나지 않아도 이상하면 똑같음.
	//	return;
	//}

	while (remain_data_size - offset >= packet_size)
	{
		if (session->GetLifeState() != LIFE_STATE::ACTIVE) break; // 현재 로직에서 disconnect 패킷이 오면 더 이상 작업을 진행하면 안된다.
		char* packet = p_buffer + offset;
		RoutePacket(packet, session);

		offset += packet_size;
		if (session->GetLifeState() != LIFE_STATE::ACTIVE) break;
		if (remain_data_size - offset < PACKET_HEADER_SIZE) break;
		packet_size = reinterpret_cast<PacketHeader*>(p_buffer + offset)->size;
	}

	session->AddDataSize(-offset);
	memmove(session->GetExOver().packet_buf, session->GetExOver().packet_buf + offset, session->GetRemainDataSize());
}

void IOCPServer::RoutePacket(char* packet, const SP<Session>& session)
{
	if (!session) return;
	PrintPacketType(reinterpret_cast<PacketHeader*>(packet)->type);
	auto room_snapshot = session->GetRoomSnapShot();
	switch (room_snapshot.state) {
	case MODE_STATE::NONE:
		return;
	case MODE_STATE::LOGIN:
		packet_handler.HandlePacket(packet, session);
		break;
	case MODE_STATE::LOBBY:
		packet_handler.HandlePacket(packet, session);
		break;
	case MODE_STATE::ROOM: {
		auto room_ptr = GetRoom(room_snapshot.room_index);
		if (!room_ptr) {
			SendError(session, ERROR_CODE::INVALID_REQUEST);
			return;
		}
		room_ptr->HandlePacket(packet, session);
		break;
	}
	}

}

void IOCPServer::BroadCastToLobby(char* packet)
{
	for (auto& user : active_users.GetActiveSessions()) {
		if (user && user->GetModeState() == MODE_STATE::LOBBY) {
			user->SendPacket(packet, iocp_handle);
		}
	}
}

//void IOCPServer::SendToSelf(char* packet, int self_index)
//{
//	users[self_index]->SendPacket(packet, iocp_handle);
//}

void IOCPServer::CreateOpenRoom(char* packet, const SP<Session>& session)
{
	if (!session) return;
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
		SendError(session, ERROR_CODE::INVALID_REQUEST);
		return;
	}
	data.room_gen = GetNewRoomGen();
	
	auto session_ptr = FindSessionByIndex(session->GetSessionKey().index);
	if (!session_ptr || session_ptr != session) return;
	SP<TetrisRoom> new_room;
	
	for (int i = 0; i < MAX_ROOM; ++i) {
		if (rooms[i].load() == nullptr) {
			data.room_index = i;
			{
				if (is_single) new_room = std::make_shared<SingleRoom>(this, data);
				else new_room = std::make_shared<MultiRoom>(this, data);
				SP<TetrisRoom> expected = nullptr;
				if (std::atomic_compare_exchange_strong(&rooms[i], &expected, new_room)) {
					if (!new_room->AddHostSession(session_ptr)) {
						SP<TetrisRoom> expected_room = new_room;
						SP<TetrisRoom> empty_room = nullptr;
						std::atomic_compare_exchange_strong(&rooms[i], &expected_room, empty_room);
						SendError(session, ERROR_CODE::INVALID_REQUEST);
						return;
					}
					active_rooms.AddRoom(data.room_gen, new_room);
					new_room->SendCreateRoom(session_ptr);
					return;
				}
			}
		}
	}

	// 나중에 방 못찾으면 추후 처리 필요
}

void IOCPServer::CreateLockRoom(char* packet, const SP<Session>& session)
{
	if (!session) return;
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
		SendError(session, ERROR_CODE::INVALID_REQUEST);
		return;
	}
	data.room_gen = GetNewRoomGen();

	auto session_ptr = FindSessionByIndex(session->GetSessionKey().index);
	if (!session_ptr || session_ptr != session) return;
	SP<TetrisRoom> new_room;

	for (int i = 0; i < MAX_ROOM; ++i) {
		if (rooms[i].load() == nullptr) {
			data.room_index = i;
			{
				if (is_single) new_room = std::make_shared<SingleRoom>(this, data);
				else new_room = std::make_shared<MultiRoom>(this, data);
				SP<TetrisRoom> expected = nullptr;
				if (std::atomic_compare_exchange_strong(&rooms[i], &expected, new_room)) {
					if (!new_room->AddHostSession(session_ptr)) {
						SP<TetrisRoom> expected_room = new_room;
						SP<TetrisRoom> empty_room = nullptr;
						std::atomic_compare_exchange_strong(&rooms[i], &expected_room, empty_room);
						SendError(session, ERROR_CODE::INVALID_REQUEST);
						return;
					}
					active_rooms.AddRoom(data.room_gen, new_room);
					new_room->SendCreateRoom(session_ptr);
					return;
				}
			}
		}
	}
}

void IOCPServer::DeleteRoom(int room_index)
{
	auto room = GetRoom(room_index);
	if (!room) return;
	active_rooms.RemoveRoom(room->GetRoomGen(), room);
	rooms[room_index].store(nullptr);
	std::cout << "방 삭제 - 방 이름: " << room->GetRoomName() << std::endl;
}

void IOCPServer::RequestLoadRanking()
{
	EnqueueDBTask(std::make_unique<DBLoadRankingTask>());
}


int IOCPServer::GetNewRoomGen()
{
	return room_gen_generator.fetch_add(1) + 1;
}

int IOCPServer::GetEmptyUserIndex()
{
	for (int i = 0; i < MAX_USER; ++i) {
		if (users[i].load() == nullptr) return i;
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

SP<TetrisRoom> IOCPServer::GetRoom(int room_index) const
{
	if (room_index < 0 || room_index >= MAX_ROOM) return nullptr;
	return rooms[room_index].load();
}

SP<Session> IOCPServer::FindSessionByIndex(int user_index)
{
	if (user_index < 0 || user_index >= MAX_USER) return nullptr;
	return users[user_index].load();
}

void IOCPServer::BeginDisconnect(const SP<Session>& session) // 첫 disconnect
{
	if (!session) return;
	auto session_ptr = FindSessionByIndex(session->GetSessionKey().index);
	active_users.RemoveUser(session->GetDBInfo().id, session_ptr);
}

void IOCPServer::TryDisconnect(const SP<Session>& session) // disconnect 대기 중인 세션 disconnect 시도
{
	Disconnect(session);
}

void IOCPServer::Disconnect(const SP<Session>& session)
{
	if (!session) return;
	int user_index = session->GetSessionKey().index;
	auto session_ptr = FindSessionByIndex(user_index);
	if (!session_ptr || session_ptr != session) return;

	int user_id = session->GetDBInfo().id;
	auto room_snapshot = session->GetRoomSnapShot();
	if (user_index >= 0 && user_index < MAX_USER) {
		if (room_snapshot.state == MODE_STATE::ROOM) {
			auto room = GetRoom(room_snapshot.room_index);
			if (room) room->DeleteUser(user_id);
		}
	}

	active_users.RemoveUser(session->GetDBInfo().id, session_ptr);
	std::cout << "로그아웃 - 플레이어: " << session->GetDBInfo().nickname << std::endl;
	closesocket(session->GetSocket());
	if (user_index >= 0 && user_index < MAX_USER) {
		SP<Session> expected_session = session_ptr;
		SP<Session> empty_session = nullptr;
		users[user_index].compare_exchange_strong(expected_session, empty_session);
	}
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

