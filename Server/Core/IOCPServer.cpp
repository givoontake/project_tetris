#include <iostream>
#include <algorithm>
#include "IOCPServer.h"
#include "SingleRoom.h"
#include "MultiRoom.h"
#include "TwoPlayerRoom.h"
#include "FivePlayerRoom.h"

#undef min

IOCPServer::IOCPServer()
	: packet_handler_(*this), db_result_handler_(*this)
{
	for (int i = 0; i < MAX_PLAYER_COUNT; ++i) {
		sessions_[i].store(nullptr);
	}

	// load는 객체 복사가 아니라 컨트롤 블록을 가리키는 핸들(shared_ptr)만 복사하는 것, 접근 흐름은 shared_ptr -> controll block(카운터, 실제 객체 포인터 등 존재) -> 실제 객체 이다.
	// 즉 참조 카운트를 늘리는 동작이며 다른 곳에서 객체를 해제해도 안전하게 동작할 수 있도록 한다. 의도된 동작은 아닐 수 있어도 수명은 확실하게 관리된다.
	// shared_ptr의 기본값은 nullptr이므로 초기화는 필요 없다.
	
	//for (int i = 0; i < MAX_ROOM_COUNT; ++i) {

	//	auto room = rooms_[i].load();
	//	room = nullptr;
	//}

	//packet_handler_ = std::make_unique<PacketHandler>(this);
	//for (auto& session : sessions_) {
	//	session = std::make_unique<Session>(packet_handler_.get()); // packet_handler_는 unique_ptr이므로 get()을 이용해 raw ptr을 넘긴다.
	//}

	WSAStartup(MAKEWORD(2, 2), &wsa_data_);

	listen_socket_ = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);
	accept_socket_ = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);
	memset(&server_addr_, 0, sizeof(server_addr_));
	server_addr_.sin_family = AF_INET;
	server_addr_.sin_port = htons(SERVER_PORT);
	server_addr_.sin_addr.S_un.S_addr = INADDR_ANY;

	accept_over_.SetOperationType(OPType::ACCEPT);

}

IOCPServer::~IOCPServer()
{
	for(auto& room : rooms_) {
		std::atomic_store(&room, SP<TetrisRoom>{}); // nullptr과 같은 논리
	}
	active_rooms_.Clear();
	active_players_.Clear();
	closesocket(listen_socket_);
	closesocket(accept_socket_);
	StopDBThreads();
	WSACleanup();
}

void IOCPServer::InitDBThreads()
{
	login_db_thread_.Init(iocp_handle_);
	for (auto& db_thread : game_db_threads_)
		db_thread.Init(iocp_handle_);
}

void IOCPServer::StartDBThreads()
{
	login_db_thread_.Start();
	for (auto& db_thread : game_db_threads_)
		db_thread.Start();
}

void IOCPServer::StopDBThreads()
{
	login_db_thread_.Close();
	for (auto& db_thread : game_db_threads_)
		db_thread.Close();
}

void IOCPServer::WakeDBThreads()
{
	login_db_thread_.Wake();
	for (auto& db_thread : game_db_threads_)
		db_thread.Wake();
}

bool IOCPServer::EnqueueDBTask(std::unique_ptr<ServerDBTask> db_task)
{
	const std::size_t thread_index = next_game_db_thread_.fetch_add(1) % game_db_threads_.size();
	return game_db_threads_[thread_index].Enqueue(std::move(db_task));
}

bool IOCPServer::EnqueueDBTask(std::unique_ptr<SessionDBTask> db_task, const SP<Session>& session)
{
	bool is_enqueued = false;
	if (db_task->operation_type == DBOperationType::LOGIN)
	{
		is_enqueued = login_db_thread_.Enqueue(std::move(db_task), session);
	}
	else
	{
		std::size_t thread_index = 0;
		if (db_task->session_key.player_id >= 0)
			thread_index = static_cast<std::size_t>(db_task->session_key.player_id) % game_db_threads_.size();
		else
			thread_index = next_game_db_thread_.fetch_add(1) % game_db_threads_.size();

		is_enqueued = game_db_threads_[thread_index].Enqueue(std::move(db_task), session);
	}

	if (!is_enqueued && session->TryDeactivate()) TryDisconnect(session);
	return is_enqueued;
}

void IOCPServer::EnqueueDBTask(std::unique_ptr<MultiSessionDBTask> db_task, const SP<Session> (&sessions)[MAX_MATCH_RESULT_PLAYERS])
{
	for (int i = 0; i < db_task->player_count; ++i)
	{
		if (sessions[i]->TryAddPending()) db_task->completion_mask |= static_cast<uint8_t>(1u << i);
		else if (sessions[i]->TryDeactivate()) TryDisconnect(sessions[i]);
	}

	const std::size_t thread_index = static_cast<std::size_t>(db_task->player_keys[0].player_id) % game_db_threads_.size();
	game_db_threads_[thread_index].Enqueue(std::move(db_task));
}


void IOCPServer::SendRoomList(const SP<Session>& session)
{
	if (!session) return;
	char packet_buffer[BUF_SIZE];
	int packet_size = 0;
	for (auto& room_sp : active_rooms_.GetActiveRoomsSnapshot()) {
		if (!room_sp) continue;
		S2C_ROOM_INFO_PACKET info_p;
		RoomInfoSnapshot room_snapshot = room_sp->GetRoomInfoSnapshot();
		if (room_snapshot.room_state != RoomState::WAIT && room_snapshot.room_state != RoomState::PLAY) continue;
		info_p.header.size = static_cast<std::uint16_t>(sizeof(info_p));
		info_p.header.type = S2C_ROOM_INFO;
		info_p.room_gen = room_snapshot.room_gen;
		info_p.max_player_count = room_snapshot.max_player_count;
		info_p.current_player_count = room_snapshot.current_player_count;
		StringToCharBuf(room_snapshot.room_name, info_p.room_name, sizeof(info_p.room_name));
		info_p.is_private = room_snapshot.is_private;
		info_p.is_play = room_snapshot.room_state == RoomState::PLAY;

		if (packet_size + sizeof(info_p) > BUF_SIZE) { 
			session->SendPacket(reinterpret_cast<char*>(packet_buffer), packet_size, iocp_handle_);
			packet_size = 0;
		}

		memcpy(packet_buffer + packet_size, &info_p, sizeof(info_p));
		packet_size += sizeof(info_p);
		//if (session->GetSessionKey().gen != request_gen) return;
	}
	session->SendPacket(reinterpret_cast<char*>(packet_buffer), packet_size, iocp_handle_);
}

// 
int IOCPServer::TryJoinRoom(const SP<Session>& session, int room_gen, const std::string& room_password, int matching_max_player_count)
{	
	if (!session) return ErrorCode::INVALID_REQUEST;
	auto session_ptr = FindSessionByIndex(session->GetSessionKey().session_index);
	if (!session_ptr || session_ptr != session) return ErrorCode::INVALID_REQUEST;
	SP<TetrisRoom> room_sp = FindRoomByGen(room_gen);
	if (!room_sp) return ErrorCode::ROOM_NOT_FOUND;
	const RoomInfoSnapshot snapshot = room_sp->GetRoomInfoSnapshot();
	if (snapshot.room_state == RoomState::WAITING_DELETE) return ErrorCode::ROOM_NOT_FOUND;
	if (snapshot.room_state == RoomState::PLAY) return ErrorCode::ROOM_IN_GAME;
	if (snapshot.current_player_count >= snapshot.max_player_count) return ErrorCode::ROOM_FULL;
	if (room_sp->GetMaxPlayerCount() == 1) return ErrorCode::INVALID_REQUEST;
	if (room_sp->IsPrivate() && room_sp->GetRoomPassword() != room_password) return ErrorCode::ROOM_INVALID_PASSWORD;
	auto multi_sp = std::dynamic_pointer_cast<MultiRoom>(room_sp); // TetrisRoom -> MultiRoom으로 다운캐스팅(참조 카운트 증가)
	if (!multi_sp) return ErrorCode::SERVER_ERROR;
	if (!session_ptr->TrySetRoomMode(room_sp->GetRoomIndex())) return ErrorCode::INVALID_REQUEST;
	if (!multi_sp->AddPlayerTask(session_ptr, matching_max_player_count)) {
		session_ptr->SetRoomSnapshot(ModeState::LOBBY, -1);
		return ErrorCode::ROOM_NOT_FOUND;
	}
	return SUCCESS;
}

SP<TetrisRoom> IOCPServer::FindRoomByGen(int room_gen)
{
	return active_rooms_.FindRoomByGen(room_gen);
}

void IOCPServer::SendError(const SP<Session>& session, int error_code)
{
	if (!session) return;
	S2C_ERROR_PACKET error_p;
	error_p.header.size = static_cast<std::uint16_t>(sizeof(error_p));
	error_p.header.type = S2C_ERROR;
	error_p.error_code = error_code;

	session->SendPacket(reinterpret_cast<char*>(&error_p), error_p.header.size, iocp_handle_);
}

void IOCPServer::FindMatch(const SP<Session>& session, int max_player_count)
{
	if (!session) return;
	if (max_player_count == 0) {
		for (auto& room : rooms_) {
			// 방에 접근할 때는 무조건 Shared_ptr을 로드해서 참조 카운트를 늘려야 한다. 방이 삭제되더라도 안전하게 동작하기 위해서이다.
			// 단순히 널을 체크하고 들어가도 그 다음 내부 객체 접근 시 그 객체가 삭제되었을 수 있다.
			auto room_sp = room.load();
			if (room_sp) {
				if (room_sp->IsPrivate()) continue;
				if (room_sp->GetMaxPlayerCount() == 2 or room_sp->GetMaxPlayerCount() == 5) { // 공개 멀티 방 중 아무 방이나 찾기
					const RoomInfoSnapshot snapshot = room_sp->GetRoomInfoSnapshot();
					if (snapshot.room_state != RoomState::WAIT || snapshot.current_player_count >= snapshot.max_player_count) continue;
					int result = TryJoinRoom(session, room_sp->GetRoomGen(), "", max_player_count);
					if (result == SUCCESS) return;
					if (result == ErrorCode::INVALID_REQUEST || result == ErrorCode::SERVER_ERROR) {
						SendError(session, result);
						return;
					}
				}
			}
		}
	}

	else if (max_player_count == 2) {
		for (auto& room : rooms_) {
			auto room_sp = room.load();
			if (room_sp) {
				if (room_sp->IsPrivate()) continue;
				if (room_sp->GetMaxPlayerCount() == max_player_count) {
					const RoomInfoSnapshot snapshot = room_sp->GetRoomInfoSnapshot();
					if (snapshot.room_state != RoomState::WAIT || snapshot.current_player_count >= snapshot.max_player_count) continue;
					int result = TryJoinRoom(session, room_sp->GetRoomGen(), "", max_player_count);
					if (result == SUCCESS) return;
					if (result == ErrorCode::INVALID_REQUEST || result == ErrorCode::SERVER_ERROR) {
						SendError(session, result);
						return;
					}
				}
			}
		}
	}

	else if (max_player_count == 5) {
		for (auto& room : rooms_) {
			auto room_sp = room.load();
			if (room_sp) {
				if (room_sp->IsPrivate()) continue;
				if (room_sp->GetMaxPlayerCount() == max_player_count) {
					const RoomInfoSnapshot snapshot = room_sp->GetRoomInfoSnapshot();
					if (snapshot.room_state != RoomState::WAIT || snapshot.current_player_count >= snapshot.max_player_count) continue;
					int result = TryJoinRoom(session, room_sp->GetRoomGen(), "", max_player_count);
					if (result == SUCCESS) return;
					if (result == ErrorCode::INVALID_REQUEST || result == ErrorCode::SERVER_ERROR) {
						SendError(session, result);
						return;
					}
				}
			}
		}
	}

	else {
		SendError(session, ErrorCode::INVALID_REQUEST);
		return;
	}
	
	SendError(session, ErrorCode::NOT_FOUND_JOINABLE_ROOM);
}

void IOCPServer::SendLobbyPlayerList(const SP<Session>& session)
{
	if (!session) return;
	{
		// 비용을 줄이기 위한 선체크
		if (session->GetModeState() != ModeState::LOBBY) return;
	}
	
	int packet_size = 0;
	char packet_buffer[BUF_SIZE];
	for (auto& player : active_players_.GetActiveSessions()) {
		S2C_LOBBY_PLAYER_INFO_PACKET info_p;
		info_p.header.size = static_cast<std::uint16_t>(sizeof(info_p));
		info_p.header.type = S2C_LOBBY_PLAYER_INFO;
		//info_p.player_id = -1;
		{
			if (player->GetDBInfo().player_id == session->GetDBInfo().player_id) continue;
			if (player->GetModeState() == ModeState::LOBBY) {
				info_p.player_id = player->GetDBInfo().player_id;
				StringToCharBuf(player->GetDBInfo().nickname, info_p.nickname, MAX_ROOM_NAME_SIZE);
			}
			else continue;
		}
		
		if (packet_size + sizeof(info_p) > BUF_SIZE) {
			session->SendPacket(reinterpret_cast<char*>(packet_buffer), packet_size, iocp_handle_);
			packet_size = 0;
		}

		memcpy(packet_buffer + packet_size, &info_p, sizeof(info_p));
		packet_size += sizeof(info_p);
	}
	session->SendPacket(reinterpret_cast<char*>(packet_buffer), packet_size, iocp_handle_);
}

void IOCPServer::SendFriendList(const SP<Session>& session)
{
	if (!session) return;
	std::vector<FriendInfo> friend_list;
	{
		if (session->GetModeState() != ModeState::LOBBY) return;
		friend_list = session->GetFriendList();
	}

	int packet_size = 0;
	char packet_buffer[BUF_SIZE];
	for (auto& friend_info : friend_list) {
		S2C_FRIEND_INFO_PACKET info_p; // 얘는 그냥 지 세션에 있는 친구 목록이라 미리 다 작성하고 현재 친구 상태만 검사해서 보내주면 됨
		info_p.header.size = static_cast<std::uint16_t>(sizeof(info_p));
		info_p.header.type = S2C_FRIEND_INFO;
		info_p.player_id = friend_info.player_id;
		info_p.is_lobby = false;
		StringToCharBuf(friend_info.nickname, info_p.nickname, MAX_PLAYER_NAME_SIZE);

		// 로비인지 체크만 함
		auto session = active_players_.FindSessionByID(friend_info.player_id);
		if (session) {
			if (session->GetDBInfo().player_id != friend_info.player_id) continue;
			if (session->GetModeState() == ModeState::LOBBY) info_p.is_lobby = true;
		}

		if (packet_size + sizeof(info_p) > BUF_SIZE) {
			session->SendPacket(reinterpret_cast<char*>(packet_buffer), packet_size, iocp_handle_);
			packet_size = 0;
		}

		memcpy(packet_buffer + packet_size, &info_p, sizeof(info_p));
		packet_size += sizeof(info_p);
	}
	session->SendPacket(reinterpret_cast<char*>(packet_buffer), packet_size, iocp_handle_);
}

void IOCPServer::SendRankings(const SP<Session>& session)
{
	if (!session) return;
	{
		if (session->GetModeState() != ModeState::LOBBY) return;
	}

	std::vector<RankingInfo> rankings = ranking_manager_.GetRankings();
	if (rankings.empty()) return;

	int packet_size = 0;
	char packet_buffer[BUF_SIZE];
	for (const auto& ranking : rankings) {
		S2C_RANKING_INFO_PACKET info_p{};
		info_p.header.size = static_cast<std::uint16_t>(sizeof(info_p));
		info_p.header.type = S2C_RANKING_INFO;
		StringToCharBuf(ranking.nickname, info_p.nickname, MAX_PLAYER_NAME_SIZE);
		info_p.score = ranking.score;

		if (packet_size + sizeof(info_p) > BUF_SIZE) {
			session->SendPacket(reinterpret_cast<char*>(packet_buffer), packet_size, iocp_handle_);
			packet_size = 0;
		}

		memcpy(packet_buffer + packet_size, &info_p, sizeof(info_p));
		packet_size += sizeof(info_p);
	}

	session->SendPacket(reinterpret_cast<char*>(packet_buffer), packet_size, iocp_handle_);
}

void IOCPServer::SendAddFriendResult(FriendInfo& requester_info, FriendInfo& acceptor_info)
{
	bool is_requester_connected = false;
	S2C_ADD_FRIEND_PACKET add_p;
	add_p.header.size = static_cast<std::uint16_t>(sizeof(add_p));
	add_p.header.type = S2C_ADD_FRIEND;

	auto requester_session = active_players_.FindSessionByID(requester_info.player_id);
	if (requester_session && requester_session->GetDBInfo().player_id == requester_info.player_id) {
		requester_session->AddFriend(acceptor_info);
		if ((requester_session->GetModeState() == ModeState::LOBBY)) is_requester_connected = true;
	}

	if (is_requester_connected) {
		add_p.friend_id = acceptor_info.player_id;
		StringToCharBuf(acceptor_info.nickname, add_p.friend_nickname, MAX_PLAYER_NAME_SIZE);
		requester_session->SendPacket(reinterpret_cast<char*>(&add_p), add_p.header.size, iocp_handle_);
	}
	
	bool is_acceptor_connected = false;
	auto acceptor_session = active_players_.FindSessionByID(acceptor_info.player_id);
	if (acceptor_session && acceptor_session->GetDBInfo().player_id == acceptor_info.player_id) {
		acceptor_session->AddFriend(requester_info);
		if (acceptor_session->GetModeState() == ModeState::LOBBY) is_acceptor_connected = true;
	}

	if (is_acceptor_connected) {
		add_p.friend_id = requester_info.player_id;
		StringToCharBuf(requester_info.nickname, add_p.friend_nickname, MAX_PLAYER_NAME_SIZE);
		acceptor_session->SendPacket(reinterpret_cast<char*>(&add_p), add_p.header.size, iocp_handle_);
	}
}

// DB는 변경이 성공한 경우에 작업 성공으로 판정하도록 했으므로 결과를 통지할 세션의 존재 유무만 체크하면 된다.
void IOCPServer::SendDeleteFriendResult(int requester_id, int target_id)
{
	// 세션은 재사용하므로 논리적으로는 포인터는 항상 유효
	bool is_requester_connected = false;
	S2C_DELETE_FRIEND_PACKET delete_p;
	delete_p.header.size = static_cast<std::uint16_t>(sizeof(delete_p));
	delete_p.header.type = S2C_DELETE_FRIEND;

	auto requester_session = active_players_.FindSessionByID(requester_id);
	if (requester_session && requester_session->GetDBInfo().player_id == requester_id) { // 안전성 + 가드
		requester_session->RemoveFriend(target_id);
		if (requester_session->GetModeState() == ModeState::LOBBY) is_requester_connected = true;
	}

	if (is_requester_connected) {
		delete_p.target_id = target_id;
		requester_session->SendPacket(reinterpret_cast<char*>(&delete_p), delete_p.header.size, iocp_handle_);
	}
	
	bool is_target_connected = false;
	auto target_session = active_players_.FindSessionByID(target_id);
	if (target_session && target_session->GetDBInfo().player_id == target_id) {
		target_session->RemoveFriend(requester_id);
		if (target_session->GetModeState() == ModeState::LOBBY)  is_target_connected = true;
	}

	if (is_target_connected) {
		delete_p.target_id = requester_id;
		target_session->SendPacket(reinterpret_cast<char*>(&delete_p), delete_p.header.size, iocp_handle_);
	}
}

void IOCPServer::StartServer()
{
	bind(listen_socket_, reinterpret_cast<sockaddr*>(&server_addr_), sizeof(server_addr_));
	listen(listen_socket_, SOMAXCONN);
	iocp_handle_ = CreateIoCompletionPort(INVALID_HANDLE_VALUE, 0, 0, 0);
	CreateIoCompletionPort(reinterpret_cast<HANDLE>(listen_socket_), iocp_handle_, LISTEN_IO_COMPLETION, 0);
	int addr_size = sizeof(SOCKADDR_IN);
	AcceptEx(listen_socket_, accept_socket_, accept_over_.packet_buffer, 0, addr_size + 16, addr_size + 16, 0, &accept_over_.ex_over.over);
}

void IOCPServer::ProcessRecvBuffer(const SP<Session>& session, int recv_bytes)
{   
	if (!session) return;
	std::uint16_t packet_size = 0;
	int offset = 0;
	char packet_buffer[BUF_SIZE];
	int remaining_data_size = 0;

	if (recv_bytes + session->GetRemainingDataSize() > BUF_SIZE) return; // 버퍼가 더 이상 없다면 종료
	else session->AdjustRemainingDataSize(recv_bytes);

	if (session->GetRemainingDataSize() < PACKET_HEADER_SIZE) return; // 처리할 최소 데이터(헤더 크기 이상)가 없다면 종료

	// 우선 사이즈 - 타입 관계는 신뢰를 전제로 간다. 보안 처리는 나중에 고민할 예정
	remaining_data_size = session->GetRemainingDataSize();
	packet_size = reinterpret_cast<PACKET_HEADER*>(session->GetRecvOver().packet_buffer)->size;
	memcpy(packet_buffer, session->GetRecvOver().packet_buffer, remaining_data_size);

	//if (packet_size < PACKET_HEADER_SIZE || packet_size > BUF_SIZE) { // 이거 반쪽짜리 방어인데?? 있으나~ 없으나~ 어차피 범위를 벗어나지 않아도 이상하면 똑같음.
	//	return;
	//}

	while (remaining_data_size - offset >= packet_size)
	{
		if (session->GetLifeState() != LifeState::ACTIVE) break; // 현재 로직에서 disconnect 패킷이 오면 더 이상 작업을 진행하면 안된다.
		char* packet = packet_buffer + offset;
		RoutePacket(packet, session);

		offset += packet_size;
		if (session->GetLifeState() != LifeState::ACTIVE) break;
		if (remaining_data_size - offset < PACKET_HEADER_SIZE) break;
		packet_size = reinterpret_cast<PACKET_HEADER*>(packet_buffer + offset)->size;
	}

	session->AdjustRemainingDataSize(-offset);
	memmove(session->GetRecvOver().packet_buffer, session->GetRecvOver().packet_buffer + offset, session->GetRemainingDataSize());
}

void IOCPServer::RoutePacket(char* packet, const SP<Session>& session)
{
	if (!session) return;
	PrintPacketType(reinterpret_cast<PACKET_HEADER*>(packet)->type);
	if (reinterpret_cast<PACKET_HEADER*>(packet)->type == C2S_DISCONNECT) {
		packet_handler_.HandlePacket(packet, session);
		return;
	}
	auto room_snapshot = session->GetRoomSnapshot();
	switch (room_snapshot.mode_state) {
	case ModeState::NONE:
		return;
	case ModeState::LOGIN:
		packet_handler_.HandlePacket(packet, session);
		break;
	case ModeState::LOBBY:
		packet_handler_.HandlePacket(packet, session);
		break;
	case ModeState::ROOM: {
		auto room_ptr = GetRoomByIndex(room_snapshot.room_index);
		if (!room_ptr) {
			SendError(session, ErrorCode::INVALID_REQUEST);
			return;
		}
		room_ptr->HandlePacket(packet, session);
		break;
	}
	}

}

void IOCPServer::BroadcastToLobby(char* packet)
{
	const int packet_size = static_cast<int>(reinterpret_cast<const PACKET_HEADER*>(packet)->size);
	for (auto& player : active_players_.GetActiveSessions()) {
		if (player && player->GetModeState() == ModeState::LOBBY) {
			player->SendPacket(packet, packet_size, iocp_handle_);
		}
	}
}

//void IOCPServer::SendToSelf(char* packet, int self_index)
//{
//	sessions_[self_index]->SendPacket(packet, reinterpret_cast<PACKET_HEADER*>(packet)->size, iocp_handle_);
//}

void IOCPServer::CreatePublicRoom(char* packet, const SP<Session>& session)
{
	if (!session) return;
	C2S_ADD_PUBLIC_ROOM_PACKET* public_p = reinterpret_cast<C2S_ADD_PUBLIC_ROOM_PACKET*>(packet);
	PublicRoomInitData data;
	constexpr size_t MIN_ROOM_NAME_LENGTH = 4;
	//std::shared_ptr<TetrisRoom> new_room;
	if (public_p->max_player_count == 1) {
		data.max_player_count = public_p->max_player_count;
	}
		
	else if (public_p->max_player_count == 2 || public_p->max_player_count == 5) {
		data.max_player_count = public_p->max_player_count;
	}
		
	else return;
	data.room_name = CharBufToString(public_p->room_name, sizeof(public_p->room_name));
	if (data.room_name.size() < MIN_ROOM_NAME_LENGTH) {
		SendError(session, ErrorCode::INVALID_REQUEST);
		return;
	}
	data.room_gen = GenerateRoomGen();
	
	auto session_ptr = FindSessionByIndex(session->GetSessionKey().session_index);
	if (!session_ptr || session_ptr != session) return;
	SP<TetrisRoom> new_room;
	
	for (int i = 0; i < MAX_ROOM_COUNT; ++i) {
		if (rooms_[i].load() == nullptr) {
			data.room_index = i;
			{
				if (data.max_player_count == 1) new_room = std::make_shared<SingleRoom>(this, data);
				else if (data.max_player_count == 2) new_room = std::make_shared<TwoPlayerRoom>(this, data);
				else new_room = std::make_shared<FivePlayerRoom>(this, data);
				SP<TetrisRoom> expected = nullptr;
				if (std::atomic_compare_exchange_strong(&rooms_[i], &expected, new_room)) {
					if (!new_room->AddHostSession(session_ptr)) {
						SP<TetrisRoom> expected_room = new_room;
						SP<TetrisRoom> empty_room = nullptr;
						std::atomic_compare_exchange_strong(&rooms_[i], &expected_room, empty_room);
						SendError(session, ErrorCode::INVALID_REQUEST);
						return;
					}
					active_rooms_.AddRoom(data.room_gen, new_room);
					new_room->SendCreateRoom(session_ptr);
					new_room->CompleteRoomInitialization();
					return;
				}
			}
		}
	}

	// 나중에 방 못찾으면 추후 처리 필요
}

void IOCPServer::CreatePrivateRoom(char* packet, const SP<Session>& session)
{
	if (!session) return;
	C2S_ADD_PRIVATE_ROOM_PACKET* private_p = reinterpret_cast<C2S_ADD_PRIVATE_ROOM_PACKET*>(packet);
	PrivateRoomInitData data;
	constexpr size_t MIN_ROOM_NAME_LENGTH = 4;
	constexpr size_t MIN_ROOM_PASSWORD_LENGTH = 4;
	//std::shared_ptr<TetrisRoom> new_room;
	if (private_p->max_player_count == 1) {
		data.max_player_count = private_p->max_player_count;
	}

	else if (private_p->max_player_count == 2 || private_p->max_player_count == 5) {
		data.max_player_count = private_p->max_player_count;
	}

	else return;
	data.room_name = CharBufToString(private_p->room_name, sizeof(private_p->room_name));
	data.room_password = CharBufToString(private_p->room_password, sizeof(private_p->room_password));
	if (data.room_name.size() < MIN_ROOM_NAME_LENGTH || data.room_password.size() < MIN_ROOM_PASSWORD_LENGTH) {
		SendError(session, ErrorCode::INVALID_REQUEST);
		return;
	}
	data.room_gen = GenerateRoomGen();

	auto session_ptr = FindSessionByIndex(session->GetSessionKey().session_index);
	if (!session_ptr || session_ptr != session) return;
	SP<TetrisRoom> new_room;

	for (int i = 0; i < MAX_ROOM_COUNT; ++i) {
		if (rooms_[i].load() == nullptr) {
			data.room_index = i;
			{
				if (data.max_player_count == 1) new_room = std::make_shared<SingleRoom>(this, data);
				else if (data.max_player_count == 2) new_room = std::make_shared<TwoPlayerRoom>(this, data);
				else new_room = std::make_shared<FivePlayerRoom>(this, data);
				SP<TetrisRoom> expected = nullptr;
				if (std::atomic_compare_exchange_strong(&rooms_[i], &expected, new_room)) {
					if (!new_room->AddHostSession(session_ptr)) {
						SP<TetrisRoom> expected_room = new_room;
						SP<TetrisRoom> empty_room = nullptr;
						std::atomic_compare_exchange_strong(&rooms_[i], &expected_room, empty_room);
						SendError(session, ErrorCode::INVALID_REQUEST);
						return;
					}
					active_rooms_.AddRoom(data.room_gen, new_room);
					new_room->SendCreateRoom(session_ptr);
					new_room->CompleteRoomInitialization();
					return;
				}
			}
		}
	}
}

void IOCPServer::DeleteRoom(int room_index)
{
	auto room = GetRoomByIndex(room_index);
	if (!room) return;
	active_rooms_.RemoveRoom(room->GetRoomGen(), room);
	rooms_[room_index].store(nullptr);
	std::cout << "방 삭제 - 방 이름: " << room->GetRoomName() << std::endl;
}

void IOCPServer::RequestLoadRankings()
{
	EnqueueDBTask(std::make_unique<DBLoadRankingsTask>());
}


int IOCPServer::GenerateRoomGen()
{
	return room_gen_generator_.fetch_add(1) + 1;
}

int IOCPServer::FindAvailableSessionIndex()
{
	for (int i = 0; i < MAX_PLAYER_COUNT; ++i) {
		if (sessions_[i].load() == nullptr) return i;
	}

	return -1;
}

SP<TetrisRoom> IOCPServer::GetRoomByIndex(int room_index) const
{
	if (room_index < 0 || room_index >= MAX_ROOM_COUNT) return nullptr;
	return rooms_[room_index].load();
}

SP<Session> IOCPServer::FindSessionByIndex(int session_index)
{
	if (session_index < 0 || session_index >= MAX_PLAYER_COUNT) return nullptr;
	return sessions_[session_index].load();
}

void IOCPServer::BeginDisconnect(const SP<Session>& session) // 첫 disconnect
{
	if (!session) return;
	auto session_ptr = FindSessionByIndex(session->GetSessionKey().session_index);
	active_players_.RemovePlayer(session->GetDBInfo().player_id, session_ptr);
}

void IOCPServer::TryDisconnect(const SP<Session>& session) // disconnect 대기 중인 세션 disconnect 시도
{
	Disconnect(session);
}

void IOCPServer::Disconnect(const SP<Session>& session)
{
	if (!session) return;
	int session_index = session->GetSessionKey().session_index;
	auto session_ptr = FindSessionByIndex(session_index);
	if (!session_ptr || session_ptr != session) return;

	auto room_snapshot = session->GetRoomSnapshot();
	if (session_index >= 0 && session_index < MAX_PLAYER_COUNT) {
		if (room_snapshot.mode_state == ModeState::ROOM) {
			auto room = GetRoomByIndex(room_snapshot.room_index);
			if (room) {
				RoomTask task;
				task.task_type = RoomTaskType::REMOVE_PLAYER;
				task.session = session_ptr;
				room->AddRoomTask(std::move(task));
			}
		}
	}

	active_players_.RemovePlayer(session->GetDBInfo().player_id, session_ptr);
	std::cout << "로그아웃 - 플레이어: " << session->GetDBInfo().nickname << std::endl;
	closesocket(session->GetSocket());
	if (session_index >= 0 && session_index < MAX_PLAYER_COUNT) {
		SP<Session> expected_session = session_ptr;
		SP<Session> empty_session = nullptr;
		sessions_[session_index].compare_exchange_strong(expected_session, empty_session);
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

