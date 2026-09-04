#include <iostream>
#include <algorithm>
#include <immintrin.h>
#include "IOCPServer.h"
#include <Windows.h>
#include <bcrypt.h>
#include "SingleRoom.h"
#include "MultiRoom.h"
#include "TwoPlayerRoom.h"
#include "FivePlayerRoom.h"
#include "DBThreadManager.h"
#include "LobbyThreadManager.h"
#include "GameThreadManager.h"
#include "lobby_tasks.h"
#include "room_lifecycle_tasks.h"
#include "session_tasks.h"

#undef min

#pragma comment(lib, "bcrypt.lib")

IOCPServer::IOCPServer()
	: packet_handler_(*this), db_result_handler_(*this)
{
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
	WSACleanup();
}

bool IOCPServer::EnqueueDBTask(std::unique_ptr<ServerDBTask> db_task)
{
	return db_thread_manager_->Enqueue(std::move(db_task));
}

bool IOCPServer::EnqueueDBTask(std::unique_ptr<SessionDBTask> db_task)
{
	return db_thread_manager_->Enqueue(std::move(db_task));
}

void IOCPServer::EnqueueDBTask(std::unique_ptr<MultiSessionDBTask> db_task)
{
	db_thread_manager_->Enqueue(std::move(db_task));
}


void IOCPServer::SendRoomList(Session* session)
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
int IOCPServer::TryJoinRoom(Session* session, int room_gen, const std::string& room_password, int matching_max_player_count)
{
	if (!session) return ErrorCode::INVALID_REQUEST;
	const SessionKey session_key = session->GetSessionKey();
	auto* session_ptr = FindSession(session_key);
	if (!session_ptr || session_ptr->GetModeState() != ModeState::LOBBY) return ErrorCode::INVALID_REQUEST;
	auto room = FindRoomByGen(room_gen);
	if (!room) return ErrorCode::ROOM_NOT_FOUND;
	if (room->GetMaxPlayerCount() == 1) return ErrorCode::INVALID_REQUEST;
	if (!BeginRoomTransition(session_key)) return ErrorCode::INVALID_REQUEST;
	auto task = std::make_unique<RoomLifecycleTask>(RoomLifecycleTaskType::JOIN_ROOM, session_key);
	task->room_gen = room_gen;
	task->room_password = room_password;
	task->matching_max_player_count = matching_max_player_count;
	EnqueueRoomLifecycleTask(std::move(task));
	return SUCCESS;
}

SP<TetrisRoom> IOCPServer::FindRoomByGen(int room_gen)
{
	return active_rooms_.FindRoomByGen(room_gen);
}

void IOCPServer::SendError(Session* session, int error_code)
{
	if (!session) return;
	S2C_ERROR_PACKET error_p;
	error_p.header.size = static_cast<std::uint16_t>(sizeof(error_p));
	error_p.header.type = S2C_ERROR;
	error_p.error_code = error_code;

	session->SendPacket(reinterpret_cast<char*>(&error_p), error_p.header.size, iocp_handle_);
}

void IOCPServer::FindMatch(Session* session, int max_player_count)
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

void IOCPServer::SendLobbyPlayerList(Session* session)
{
	if (!session) return;
	{
		// 비용을 줄이기 위한 선체크
		if (session->GetModeState() != ModeState::LOBBY) return;
	}
	
	int packet_size = 0;
	char packet_buffer[BUF_SIZE];
	for (const SessionKey session_key : active_players_.GetActiveSessionKeys()) {
		auto* player = FindSession(session_key);
		if (!player) continue;
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

void IOCPServer::SendFriendList(Session* session)
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
		auto* friend_session = FindSession(active_players_.FindSessionKeyByID(friend_info.player_id));
		if (friend_session) {
			if (friend_session->GetDBInfo().player_id != friend_info.player_id) continue;
			if (friend_session->GetModeState() == ModeState::LOBBY) info_p.is_lobby = true;
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

void IOCPServer::SendRankings(Session* session)
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

	auto* requester_session = FindSession(active_players_.FindSessionKeyByID(requester_info.player_id));
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
	auto* acceptor_session = FindSession(active_players_.FindSessionKeyByID(acceptor_info.player_id));
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

	auto* requester_session = FindSession(active_players_.FindSessionKeyByID(requester_id));
	if (requester_session && requester_session->GetDBInfo().player_id == requester_id) { // 안전성 + 가드
		requester_session->RemoveFriend(target_id);
		if (requester_session->GetModeState() == ModeState::LOBBY) is_requester_connected = true;
	}

	if (is_requester_connected) {
		delete_p.target_id = target_id;
		requester_session->SendPacket(reinterpret_cast<char*>(&delete_p), delete_p.header.size, iocp_handle_);
	}
	
	bool is_target_connected = false;
	auto* target_session = FindSession(active_players_.FindSessionKeyByID(target_id));
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

bool IOCPServer::ProcessRecvBuffer(Session* session, int recv_bytes)
{   
	if (!session) return false;
	const SessionKey session_key = session->GetSessionKey();
	std::uint16_t packet_size = 0;
	int offset = 0;
	char packet_buffer[BUF_SIZE];
	int remaining_data_size = 0;
	bool should_receive = true;

	if (recv_bytes + session->GetRemainingDataSize() > BUF_SIZE) return true; // 버퍼가 더 이상 없다면 종료
	else session->AdjustRemainingDataSize(recv_bytes);

	if (session->GetRemainingDataSize() < PACKET_HEADER_SIZE) return true; // 처리할 최소 데이터(헤더 크기 이상)가 없다면 종료

	// 우선 사이즈 - 타입 관계는 신뢰를 전제로 간다. 보안 처리는 나중에 고민할 예정
	remaining_data_size = session->GetRemainingDataSize();
	packet_size = reinterpret_cast<PACKET_HEADER*>(session->GetRecvOver().packet_buffer)->size;
	memcpy(packet_buffer, session->GetRecvOver().packet_buffer, remaining_data_size);

	//if (packet_size < PACKET_HEADER_SIZE || packet_size > BUF_SIZE) { // 이거 반쪽짜리 방어인데?? 있으나~ 없으나~ 어차피 범위를 벗어나지 않아도 이상하면 똑같음.
	//	return;
	//}

	while (remaining_data_size - offset >= packet_size)
	{
		char* packet = packet_buffer + offset;
		const bool is_disconnect_packet = reinterpret_cast<PACKET_HEADER*>(packet)->type == C2S_DISCONNECT;
		std::unique_ptr<SessionTask> task;
		if (is_disconnect_packet) task = std::make_unique<SessionDisconnectTask>();
		else task = std::make_unique<SessionPacketTask>(packet, packet_size);
		if (!EnqueueSessionTask(session_key, std::move(task))) {
			should_receive = false;
			break;
		}

		offset += packet_size;
		if (is_disconnect_packet) {
			should_receive = false;
			break;
		}
		if (remaining_data_size - offset < PACKET_HEADER_SIZE) break;
		packet_size = reinterpret_cast<PACKET_HEADER*>(packet_buffer + offset)->size;
	}

	session->AdjustRemainingDataSize(-offset);
	memmove(session->GetRecvOver().packet_buffer, session->GetRecvOver().packet_buffer + offset, session->GetRemainingDataSize());
	return should_receive;
}

bool IOCPServer::EnqueueSessionTask(SessionKey session_key, std::unique_ptr<SessionTask> task)
{
	if (!task) return false;
	auto* session = FindSession(session_key);
	if (!session) return false;
	task->session_key = session_key;
	return session->TryEnqueueTask(session_key, std::move(task));
}

void IOCPServer::EnqueueLobbyTask(std::unique_ptr<LobbyTask> task)
{
	lobby_thread_manager_->Enqueue(std::move(task));
}

void IOCPServer::EnqueueRoomLifecycleTask(std::unique_ptr<RoomLifecycleTask> task)
{
	game_thread_manager_->Enqueue(std::move(task));
}

bool IOCPServer::BeginRoomTransition(SessionKey session_key)
{
	auto* session = FindSession(session_key);
	if (!session || session->GetLifeState() != LifeState::ACTIVE || session->GetModeState() != ModeState::LOBBY) return false;
	auto* lobby_session = lobby_thread_manager_->GetLobbySession(session_key.session_index);
	return lobby_session && lobby_session->TrySetPending(session_key);
}

SessionTaskProcessResult IOCPServer::ProcessSessionTask(Session* session, std::unique_ptr<SessionTask> task)
{
	if (!session || !task || !session->MatchesSessionKey(task->session_key)) return SessionTaskProcessResult::CONTINUE;
	switch (task->task_type) {
	case SessionTaskType::PACKET: {
		auto* packet_task = static_cast<SessionPacketTask*>(task.get());
		const std::uint8_t packet_type = reinterpret_cast<PACKET_HEADER*>(packet_task->packet.data())->type;
		RoutePacket(packet_task->packet.data(), session);
		if (packet_type == C2S_REMOVE_PLAYER) return SessionTaskProcessResult::RESTORE_REMAINING;
		break;
	}
	case SessionTaskType::DB_RESULT: {
		auto* db_task = static_cast<SessionDBResultTask*>(task.get());
		db_result_handler_.HandleSessionDBResult(db_task->db_over.get(), session);
		break;
	}
	case SessionTaskType::ADD_FRIEND: {
		auto* friend_task = static_cast<SessionAddFriendTask*>(task.get());
		session->AddFriend(friend_task->friend_info);
		if (session->GetModeState() == ModeState::LOBBY) {
			S2C_ADD_FRIEND_PACKET packet;
			packet.header.size = static_cast<std::uint16_t>(sizeof(packet));
			packet.header.type = S2C_ADD_FRIEND;
			packet.friend_id = friend_task->friend_info.player_id;
			StringToCharBuf(friend_task->friend_info.nickname, packet.friend_nickname, MAX_PLAYER_NAME_SIZE);
			session->SendPacket(reinterpret_cast<char*>(&packet), packet.header.size, iocp_handle_);
		}
		break;
	}
	case SessionTaskType::DELETE_FRIEND: {
		auto* friend_task = static_cast<SessionDeleteFriendTask*>(task.get());
		session->RemoveFriend(friend_task->target_player_id);
		if (session->GetModeState() == ModeState::LOBBY) {
			S2C_DELETE_FRIEND_PACKET packet;
			packet.header.size = static_cast<std::uint16_t>(sizeof(packet));
			packet.header.type = S2C_DELETE_FRIEND;
			packet.target_id = friend_task->target_player_id;
			session->SendPacket(reinterpret_cast<char*>(&packet), packet.header.size, iocp_handle_);
		}
		break;
	}
	case SessionTaskType::FRIEND_REQUEST: {
		auto* friend_task = static_cast<SessionFriendRequestTask*>(task.get());
		if (session->GetModeState() == ModeState::LOBBY) {
			S2C_ADD_FRIEND_REQUEST_PACKET packet;
			packet.header.size = static_cast<std::uint16_t>(sizeof(packet));
			packet.header.type = S2C_ADD_FRIEND_REQUEST;
			packet.requester_id = friend_task->requester_info.player_id;
			StringToCharBuf(friend_task->requester_info.nickname, packet.requester_nickname, MAX_PLAYER_NAME_SIZE);
			session->SendPacket(reinterpret_cast<char*>(&packet), packet.header.size, iocp_handle_);
		}
		break;
	}
	case SessionTaskType::DISCONNECT:
		if (session->BeginDisconnect(task->session_key))
			EnqueueLobbyTask(std::make_unique<LobbyTask>(LobbyTaskType::DISCONNECT, task->session_key));
		return SessionTaskProcessResult::DISCARD_REMAINING;
	}
	return SessionTaskProcessResult::CONTINUE;
}

void IOCPServer::ProcessLobbyTask(std::unique_ptr<LobbyTask> task)
{
	if (!task) return;
	auto* lobby_session = lobby_thread_manager_->GetLobbySession(task->session_key.session_index);
	switch (task->task_type) {
	case LobbyTaskType::ADD_SESSION: {
		auto* session = FindSession(task->session_key);
		if (!lobby_session || !session || session->GetLifeState() != LifeState::ACTIVE) return;
		const ModeState mode_state = session->GetModeState();
		if (mode_state != ModeState::LOGIN && mode_state != ModeState::LOBBY) return;
		if (lobby_session->TryPrepare(task->session_key)) lobby_session->Activate(task->session_key);
		break;
	}
	case LobbyTaskType::ROOM_TRANSITION_RESULT: {
		if (!lobby_session) return;
		if (task->result == SUCCESS) {
			lobby_session->Clear(task->session_key);
			return;
		}
		auto* session = FindSession(task->session_key);
		if (!session) {
			lobby_session->Clear(task->session_key);
			return;
		}
		if (!lobby_session->Activate(task->session_key)) return;
		if (task->matching_max_player_count >= 0) FindMatch(session, task->matching_max_player_count);
		else SendError(session, task->result);
		break;
	}
	case LobbyTaskType::ENTER_LOBBY: {
		int result = ErrorCode::INVALID_REQUEST;
		auto* session = FindSession(task->session_key);
		bool is_processing = false;
		if (lobby_session && session && session->GetLifeState() == LifeState::ACTIVE) {
			if (!session->TryStartTaskProcessing()) {
				EnqueueLobbyTask(std::move(task));
				return;
			}
			is_processing = true;
			const RoomSnapshot room_snapshot = session->GetRoomSnapshot();
			if (room_snapshot.mode_state == ModeState::ROOM && room_snapshot.room_index == task->room_index && lobby_session->TryPrepare(task->session_key)) {
				if (session->TrySetLobbyMode(task->session_key)) result = SUCCESS;
				else lobby_session->Clear(task->session_key);
			}
		}

		auto room_task = std::make_unique<RoomLifecycleTask>(RoomLifecycleTaskType::LOBBY_TRANSITION_RESULT, task->session_key);
		room_task->exit_type = task->exit_type;
		room_task->room_index = task->room_index;
		room_task->room_gen = task->room_gen;
		room_task->result = result;
		EnqueueRoomLifecycleTask(std::move(room_task));
		if (result == SUCCESS) lobby_session->Activate(task->session_key);
		if (is_processing) session->CompleteTaskProcessing();
		break;
	}
	case LobbyTaskType::DISCONNECT:
		if (!FinalizeDisconnect(task->session_key)) EnqueueLobbyTask(std::move(task));
		break;
	}
}

void IOCPServer::ProcessRoomLifecycleTask(std::unique_ptr<RoomLifecycleTask> task)
{
	if (!task) return;
	if (task->task_type == RoomLifecycleTaskType::DELETE_ROOM) {
		DeleteRoom(task->room_index);
		return;
	}
	if (task->task_type == RoomLifecycleTaskType::LOBBY_TRANSITION_RESULT) {
		auto room = GetRoomByIndex(task->room_index);
		if (!room || room->GetRoomGen() != task->room_gen) return;
		RoomProcessState expected_state = RoomProcessState::COMPLETE;
		while (!room->processing_state_.compare_exchange_weak(expected_state, RoomProcessState::PROCESSING)) {
			if (!IsRunning()) return;
			expected_state = RoomProcessState::COMPLETE;
			_mm_pause();
		}
		room->CompleteLobbyTransition(task->session_key, task->result, task->exit_type);
		room->processing_state_.store(RoomProcessState::COMPLETE);
		return;
	}
	if (task->task_type == RoomLifecycleTaskType::JOIN_ROOM) {
		int result = ErrorCode::ROOM_NOT_FOUND;
		auto room = FindRoomByGen(task->room_gen);
		if (room) {
			RoomProcessState expected_state = RoomProcessState::COMPLETE;
			while (!room->processing_state_.compare_exchange_weak(expected_state, RoomProcessState::PROCESSING)) {
				if (!IsRunning()) return;
				expected_state = RoomProcessState::COMPLETE;
				_mm_pause();
			}

			auto* session = FindSession(task->session_key);
			if (session && session->GetLifeState() == LifeState::ACTIVE) {
				while (!session->TryStartTaskProcessing()) {
					if (!IsRunning()) {
						room->processing_state_.store(RoomProcessState::COMPLETE);
						return;
					}
					_mm_pause();
				}
				auto multi_room = std::dynamic_pointer_cast<MultiRoom>(room);
				if (multi_room && session->MatchesSessionKey(task->session_key) && session->GetModeState() == ModeState::LOBBY)
					result = multi_room->AddPlayer(session, task->session_key, task->room_password);

				auto lobby_task = std::make_unique<LobbyTask>(LobbyTaskType::ROOM_TRANSITION_RESULT, task->session_key);
				lobby_task->result = result;
				lobby_task->matching_max_player_count = task->matching_max_player_count;
				EnqueueLobbyTask(std::move(lobby_task));
				if (result == SUCCESS) room->ActivatePlayer(task->session_key);
				session->CompleteTaskProcessing();
				room->processing_state_.store(RoomProcessState::COMPLETE);
				return;
			}
			room->processing_state_.store(RoomProcessState::COMPLETE);
		}

		auto lobby_task = std::make_unique<LobbyTask>(LobbyTaskType::ROOM_TRANSITION_RESULT, task->session_key);
		lobby_task->result = result;
		lobby_task->matching_max_player_count = task->matching_max_player_count;
		EnqueueLobbyTask(std::move(lobby_task));
		return;
	}

	auto* session = FindSession(task->session_key);
	if (!session || session->GetLifeState() != LifeState::ACTIVE) {
		auto lobby_task = std::make_unique<LobbyTask>(LobbyTaskType::ROOM_TRANSITION_RESULT, task->session_key);
		lobby_task->result = ErrorCode::INVALID_REQUEST;
		EnqueueLobbyTask(std::move(lobby_task));
		return;
	}
	while (!session->TryStartTaskProcessing()) {
		if (!IsRunning()) return;
		_mm_pause();
	}
	if (!session->MatchesSessionKey(task->session_key) || session->GetLifeState() != LifeState::ACTIVE || session->GetModeState() != ModeState::LOBBY) {
		session->CompleteTaskProcessing();
		auto lobby_task = std::make_unique<LobbyTask>(LobbyTaskType::ROOM_TRANSITION_RESULT, task->session_key);
		lobby_task->result = ErrorCode::INVALID_REQUEST;
		EnqueueLobbyTask(std::move(lobby_task));
		return;
	}

	int result = ErrorCode::INVALID_REQUEST;
	switch (task->task_type) {
	case RoomLifecycleTaskType::CREATE_PUBLIC:
		result = CreatePublicRoom(task->packet.data(), session, task->session_key);
		break;
	case RoomLifecycleTaskType::CREATE_PRIVATE:
		result = CreatePrivateRoom(task->packet.data(), session, task->session_key);
		break;
	default:
		break;
	}
	auto lobby_task = std::make_unique<LobbyTask>(LobbyTaskType::ROOM_TRANSITION_RESULT, task->session_key);
	lobby_task->result = result;
	EnqueueLobbyTask(std::move(lobby_task));
	if (result == SUCCESS) {
		const RoomSnapshot room_snapshot = session->GetRoomSnapshot();
		auto room = GetRoomByIndex(room_snapshot.room_index);
		if (room) room->CompleteRoomInitialization();
	}
	session->CompleteTaskProcessing();
}

void IOCPServer::RoutePacket(char* packet, Session* session)
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
	for (const SessionKey session_key : active_players_.GetActiveSessionKeys()) {
		auto* player = FindSession(session_key);
		if (player && player->GetModeState() == ModeState::LOBBY) {
			player->SendPacket(packet, packet_size, iocp_handle_);
		}
	}
}

//void IOCPServer::SendToSelf(char* packet, int self_index)
//{
//	sessions_[self_index]->SendPacket(packet, reinterpret_cast<PACKET_HEADER*>(packet)->size, iocp_handle_);
//}

int IOCPServer::CreatePublicRoom(char* packet, Session* session, SessionKey session_key)
{
	if (!session || !packet) return ErrorCode::INVALID_REQUEST;
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
		
	else return ErrorCode::INVALID_REQUEST;
	data.room_name = CharBufToString(public_p->room_name, sizeof(public_p->room_name));
	if (data.room_name.size() < MIN_ROOM_NAME_LENGTH) return ErrorCode::INVALID_REQUEST;
	data.room_gen = GenerateRoomGen();
	
	auto* session_ptr = FindSession(session->GetSessionKey());
	if (!session_ptr) return ErrorCode::INVALID_REQUEST;
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
					if (!new_room->AddHostSession(session_ptr, session_key)) {
						SP<TetrisRoom> expected_room = new_room;
						SP<TetrisRoom> empty_room = nullptr;
						std::atomic_compare_exchange_strong(&rooms_[i], &expected_room, empty_room);
						return ErrorCode::INVALID_REQUEST;
					}
					active_rooms_.AddRoom(data.room_gen, new_room);
					new_room->SendCreateRoom(session_ptr);
					return SUCCESS;
				}
			}
		}
	}

	// 나중에 방 못찾으면 추후 처리 필요
	return ErrorCode::SERVER_ERROR;
}

int IOCPServer::CreatePrivateRoom(char* packet, Session* session, SessionKey session_key)
{
	if (!session || !packet) return ErrorCode::INVALID_REQUEST;
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

	else return ErrorCode::INVALID_REQUEST;
	data.room_name = CharBufToString(private_p->room_name, sizeof(private_p->room_name));
	data.room_password = CharBufToString(private_p->room_password, sizeof(private_p->room_password));
	if (data.room_name.size() < MIN_ROOM_NAME_LENGTH || data.room_password.size() < MIN_ROOM_PASSWORD_LENGTH) return ErrorCode::INVALID_REQUEST;
	data.room_gen = GenerateRoomGen();

	auto* session_ptr = FindSession(session->GetSessionKey());
	if (!session_ptr) return ErrorCode::INVALID_REQUEST;
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
					if (!new_room->AddHostSession(session_ptr, session_key)) {
						SP<TetrisRoom> expected_room = new_room;
						SP<TetrisRoom> empty_room = nullptr;
						std::atomic_compare_exchange_strong(&rooms_[i], &expected_room, empty_room);
						return ErrorCode::INVALID_REQUEST;
					}
					active_rooms_.AddRoom(data.room_gen, new_room);
					new_room->SendCreateRoom(session_ptr);
					return SUCCESS;
				}
			}
		}
	}
	return ErrorCode::SERVER_ERROR;
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

Session* IOCPServer::AcquireSession(SOCKET new_socket)
{
	const std::uint64_t session_id = GenerateSessionID();
	if (session_id == 0) return nullptr;
	for (int i = 0; i < MAX_PLAYER_COUNT; ++i) {
		if (sessions_[i].InitSession(i, session_id, new_socket)) {
			const SessionKey session_key = sessions_[i].GetSessionKey();
			EnqueueLobbyTask(std::make_unique<LobbyTask>(LobbyTaskType::ADD_SESSION, session_key));
			return &sessions_[i];
		}
	}
	return nullptr;
}

std::uint64_t IOCPServer::GenerateSessionID()
{
	std::uint64_t session_id = 0;
	while (session_id == 0) {
		if (BCryptGenRandom(nullptr, reinterpret_cast<PUCHAR>(&session_id), sizeof(session_id), BCRYPT_USE_SYSTEM_PREFERRED_RNG) < 0) return 0;
	}
	return session_id;
}

SP<TetrisRoom> IOCPServer::GetRoomByIndex(int room_index) const
{
	if (room_index < 0 || room_index >= MAX_ROOM_COUNT) return nullptr;
	return rooms_[room_index].load();
}

Session* IOCPServer::FindSessionByIndex(int session_index)
{
	if (session_index < 0 || session_index >= MAX_PLAYER_COUNT) return nullptr;
	auto* session = &sessions_[session_index];
	const LifeState life_state = session->GetLifeState();
	if (life_state == LifeState::NONE || life_state == LifeState::INITIALIZING) return nullptr;
	return session;
}

Session* IOCPServer::FindSession(SessionKey session_key)
{
	auto* session = FindSessionByIndex(session_key.session_index);
	if (!session || !session->MatchesSessionKey(session_key)) return nullptr;
	return session;
}

void IOCPServer::RequestDisconnect(SessionKey session_key)
{
	EnqueueSessionTask(session_key, std::make_unique<SessionDisconnectTask>());
}

void IOCPServer::CompleteSessionIO(SessionKey session_key)
{
	auto* session = FindSession(session_key);
	if (session && session->CompleteIO(session_key))
		EnqueueLobbyTask(std::make_unique<LobbyTask>(LobbyTaskType::DISCONNECT, session_key));
}

void IOCPServer::CompleteRoomDisconnect(SessionKey session_key)
{
	auto* session = FindSession(session_key);
	if (session && session->CompleteRoomRemoval(session_key))
		EnqueueLobbyTask(std::make_unique<LobbyTask>(LobbyTaskType::DISCONNECT, session_key));
}

bool IOCPServer::FinalizeDisconnect(SessionKey session_key)
{
	auto* session = FindSession(session_key);
	if (!session || session->GetLifeState() != LifeState::DISCONNECTING) return true;
	if (!session->TryStartTaskProcessing()) return false;
	if (!session->MatchesSessionKey(session_key) || session->GetLifeState() != LifeState::DISCONNECTING) {
		session->CompleteTaskProcessing();
		return true;
	}

	const SessionKey current_session_key = session->GetSessionKey();
	const RoomSnapshot room_snapshot = session->GetRoomSnapshot();
	if (room_snapshot.mode_state == ModeState::ROOM) {
		auto room = GetRoomByIndex(room_snapshot.room_index);
		if (room) {
			RoomTask task;
			task.task_type = RoomTaskType::REMOVE_PLAYER;
			task.session_key = current_session_key;
			const bool is_enqueued = room->AddRoomTask(std::move(task));
			session->CompleteTaskProcessing();
			return is_enqueued;
		}
		CompleteRoomDisconnect(current_session_key);
		session->CompleteTaskProcessing();
		return true;
	}

	const DBResultLogin db_info = session->GetDBInfo();
	active_players_.RemovePlayer(db_info.player_id, current_session_key);
	std::cout << "로그아웃 - 플레이어: " << db_info.nickname << std::endl;
	auto* lobby_session = lobby_thread_manager_->GetLobbySession(current_session_key.session_index);
	if (lobby_session) lobby_session->Clear(current_session_key);
	session->DiscardTasks();
	session->CompleteTaskProcessing();
	session->FinalizeDisconnect(current_session_key);
	return true;
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

