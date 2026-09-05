#include <algorithm>
#include <iostream>
#include "TetrisServer.h"
#include <Windows.h>
#include <bcrypt.h>
#include "ServerThreadManager.h"
#include "common_packets.h"
#include "database_packets.h"
#include "lobby_tasks.h"
#include "packet_types.h"
#include "room_lifecycle_tasks.h"
#include "session_tasks.h"

#undef min

#pragma comment(lib, "bcrypt.lib")

TetrisServer::TetrisServer()
	: db_result_handler_(*this)
{
	WSAStartup(MAKEWORD(2, 2), &wsa_data_);

	listen_socket_ = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);
	accept_socket_ = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);
	memset(&server_addr_, 0, sizeof(server_addr_));
	server_addr_.sin_family = AF_INET;
	server_addr_.sin_port = htons(SERVER_PORT);
	server_addr_.sin_addr.S_un.S_addr = INADDR_ANY;

	accept_over_.SetOperationType(OPType::ACCEPT);

}

TetrisServer::~TetrisServer()
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

bool TetrisServer::EnqueueDBTask(std::unique_ptr<ServerDBTask> db_task)
{
	return thread_manager_->GetDBThreadManager().Enqueue(std::move(db_task));
}

bool TetrisServer::EnqueueDBTask(std::unique_ptr<SessionDBTask> db_task)
{
	return thread_manager_->GetDBThreadManager().Enqueue(std::move(db_task));
}

void TetrisServer::EnqueueDBTask(std::unique_ptr<MultiSessionDBTask> db_task)
{
	thread_manager_->GetDBThreadManager().Enqueue(std::move(db_task));
}


void TetrisServer::SendError(Session& session, int error_code)
{
	S2C_ERROR_PACKET error_p;
	error_p.header.size = static_cast<std::uint16_t>(sizeof(error_p));
	error_p.header.type = S2C_ERROR;
	error_p.error_code = error_code;

	session.SendPacket(reinterpret_cast<char*>(&error_p), error_p.header.size, iocp_handle_);
}

void TetrisServer::StartServer()
{
	bind(listen_socket_, reinterpret_cast<sockaddr*>(&server_addr_), sizeof(server_addr_));
	listen(listen_socket_, SOMAXCONN);
	iocp_handle_ = CreateIoCompletionPort(INVALID_HANDLE_VALUE, 0, 0, 0);
	CreateIoCompletionPort(reinterpret_cast<HANDLE>(listen_socket_), iocp_handle_, LISTEN_IO_COMPLETION, 0);
	int addr_size = sizeof(SOCKADDR_IN);
	AcceptEx(listen_socket_, accept_socket_, accept_over_.packet_buffer, 0, addr_size + 16, addr_size + 16, 0, &accept_over_.ex_over.over);
}

void TetrisServer::CompleteAccept()
{
	auto* new_session = AcquireSession(accept_socket_);
	if (new_session) {
		CreateIoCompletionPort(reinterpret_cast<HANDLE>(accept_socket_), iocp_handle_, SESSION_IO_COMPLETION, 0);
		new_session->RecvPacket(iocp_handle_);
	}
	else {
		closesocket(accept_socket_);
	}
	accept_socket_ = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);

	if (accept_socket_ != INVALID_SOCKET) {
		ZeroMemory(&accept_over_.ex_over.over, sizeof(accept_over_.ex_over.over));
		int addr_size = sizeof(SOCKADDR_IN);
		BOOL result = AcceptEx(listen_socket_, accept_socket_, accept_over_.packet_buffer, 0, addr_size + 16, addr_size + 16, 0, &accept_over_.ex_over.over);
		if (!result && WSAGetLastError() != ERROR_IO_PENDING) std::cerr << "AcceptEx fail.. " << std::endl;
	}
}

void TetrisServer::StartThreads()
{
	thread_manager_ = std::make_unique<ServerThreadManager>(*this);
	thread_manager_->StartThreads();
}

void TetrisServer::CloseThreads()
{
	if (thread_manager_) thread_manager_->CloseThreads();
}

void TetrisServer::JoinThreads()
{
	if (thread_manager_) thread_manager_->JoinThreads();
}

bool TetrisServer::ProcessRecvBuffer(Session& session, SessionKey session_key, int recv_bytes)
{   
	std::uint16_t packet_size = 0;
	int offset = 0;
	char packet_buffer[BUF_SIZE];
	int remaining_data_size = 0;
	bool should_receive = true;

	if (recv_bytes + session.GetRemainingDataSize() > BUF_SIZE) return true; // 버퍼가 더 이상 없다면 종료
	else session.AdjustRemainingDataSize(recv_bytes);

	if (session.GetRemainingDataSize() < PACKET_HEADER_SIZE) return true; // 처리할 최소 데이터(헤더 크기 이상)가 없다면 종료

	// 우선 사이즈 - 타입 관계는 신뢰를 전제로 간다. 보안 처리는 나중에 고민할 예정
	remaining_data_size = session.GetRemainingDataSize();
	packet_size = reinterpret_cast<PACKET_HEADER*>(session.GetRecvOver().packet_buffer)->size;
	memcpy(packet_buffer, session.GetRecvOver().packet_buffer, remaining_data_size);

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

	session.AdjustRemainingDataSize(-offset);
	memmove(session.GetRecvOver().packet_buffer, session.GetRecvOver().packet_buffer + offset, session.GetRemainingDataSize());
	return should_receive;
}

bool TetrisServer::EnqueueSessionTask(SessionKey session_key, std::unique_ptr<SessionTask> task)
{
	if (!task) return false;
	auto* session = FindSession(session_key);
	if (!session) return false;
	task->session_key = session_key;
	return session->TryEnqueueTask(session_key, std::move(task));
}

void TetrisServer::EnqueueLobbyTask(std::unique_ptr<LobbyTask> task)
{
	thread_manager_->GetLobbyThreadManager().Enqueue(std::move(task));
}

void TetrisServer::EnqueueRoomLifecycleTask(std::unique_ptr<RoomLifecycleTask> task)
{
	thread_manager_->GetGameThreadManager().Enqueue(std::move(task));
}

SessionTaskProcessResult TetrisServer::ProcessSessionTask(Session& session, std::unique_ptr<SessionTask> task)
{
	if (!task || !session.MatchesSessionKey(task->session_key)) return SessionTaskProcessResult::CONTINUE;
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
		session.AddFriend(friend_task->friend_info);
		if (session.GetModeState() == ModeState::LOBBY) {
			S2C_ADD_FRIEND_PACKET packet;
			packet.header.size = static_cast<std::uint16_t>(sizeof(packet));
			packet.header.type = S2C_ADD_FRIEND;
			packet.friend_id = friend_task->friend_info.player_id;
			StringToCharBuf(friend_task->friend_info.nickname, packet.friend_nickname, MAX_PLAYER_NAME_SIZE);
			session.SendPacket(reinterpret_cast<char*>(&packet), packet.header.size, iocp_handle_);
		}
		break;
	}
	case SessionTaskType::DELETE_FRIEND: {
		auto* friend_task = static_cast<SessionDeleteFriendTask*>(task.get());
		session.RemoveFriend(friend_task->target_player_id);
		if (session.GetModeState() == ModeState::LOBBY) {
			S2C_DELETE_FRIEND_PACKET packet;
			packet.header.size = static_cast<std::uint16_t>(sizeof(packet));
			packet.header.type = S2C_DELETE_FRIEND;
			packet.target_id = friend_task->target_player_id;
			session.SendPacket(reinterpret_cast<char*>(&packet), packet.header.size, iocp_handle_);
		}
		break;
	}
	case SessionTaskType::FRIEND_REQUEST: {
		auto* friend_task = static_cast<SessionFriendRequestTask*>(task.get());
		if (session.GetModeState() == ModeState::LOBBY) {
			S2C_ADD_FRIEND_REQUEST_PACKET packet;
			packet.header.size = static_cast<std::uint16_t>(sizeof(packet));
			packet.header.type = S2C_ADD_FRIEND_REQUEST;
			packet.requester_id = friend_task->requester_info.player_id;
			StringToCharBuf(friend_task->requester_info.nickname, packet.requester_nickname, MAX_PLAYER_NAME_SIZE);
			session.SendPacket(reinterpret_cast<char*>(&packet), packet.header.size, iocp_handle_);
		}
		break;
	}
	case SessionTaskType::DISCONNECT:
		if (session.BeginDisconnect(task->session_key))
			EnqueueLobbyTask(std::make_unique<LobbyTask>(LobbyTaskType::DISCONNECT, task->session_key));
		return SessionTaskProcessResult::DISCARD_REMAINING;
	}
	return SessionTaskProcessResult::CONTINUE;
}

void TetrisServer::RoutePacket(char* packet, Session& session)
{
	if (reinterpret_cast<PACKET_HEADER*>(packet)->type == C2S_DISCONNECT) {
		RequestDisconnect(session.GetSessionKey());
		return;
	}
	auto room_snapshot = session.GetRoomSnapshot();
	switch (room_snapshot.mode_state) {
	case ModeState::NONE:
		return;
	case ModeState::LOGIN:
	case ModeState::LOBBY:
		thread_manager_->GetLobbyThreadManager().ProcessPacket(packet, session);
		break;
	case ModeState::ROOM:
		thread_manager_->GetGameThreadManager().ProcessPacket(packet, session);
		break;
	}

}

void TetrisServer::RequestLoadRankings()
{
	EnqueueDBTask(std::make_unique<DBLoadRankingsTask>());
}


Session* TetrisServer::AcquireSession(SOCKET new_socket)
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

std::uint64_t TetrisServer::GenerateSessionID()
{
	std::uint64_t session_id = 0;
	while (session_id == 0) {
		if (BCryptGenRandom(nullptr, reinterpret_cast<PUCHAR>(&session_id), sizeof(session_id), BCRYPT_USE_SYSTEM_PREFERRED_RNG) < 0) return 0;
	}
	return session_id;
}

SP<TetrisRoom> TetrisServer::GetRoomByIndex(int room_index) const
{
	if (room_index < 0 || room_index >= MAX_ROOM_COUNT) return nullptr;
	return rooms_[room_index].load();
}

bool TetrisServer::TryAddRoom(int room_index, const SP<TetrisRoom>& room)
{
	if (room_index < 0 || room_index >= MAX_ROOM_COUNT || !room) return false;
	SP<TetrisRoom> expected_room;
	return std::atomic_compare_exchange_strong(&rooms_[room_index], &expected_room, room);
}

bool TetrisServer::TryRemoveRoom(int room_index, const SP<TetrisRoom>& room)
{
	if (room_index < 0 || room_index >= MAX_ROOM_COUNT || !room) return false;
	SP<TetrisRoom> expected_room = room;
	return std::atomic_compare_exchange_strong(&rooms_[room_index], &expected_room, SP<TetrisRoom>{});
}

int TetrisServer::GenerateRoomGen()
{
	return room_gen_generator_.fetch_add(1) + 1;
}

Session* TetrisServer::FindSessionByIndex(int session_index)
{
	if (session_index < 0 || session_index >= MAX_PLAYER_COUNT) return nullptr;
	auto* session = &sessions_[session_index];
	const LifeState life_state = session->GetLifeState();
	if (life_state == LifeState::NONE || life_state == LifeState::INITIALIZING) return nullptr;
	return session;
}

Session* TetrisServer::FindSession(SessionKey session_key)
{
	auto* session = FindSessionByIndex(session_key.session_index);
	if (!session || !session->MatchesSessionKey(session_key)) return nullptr;
	return session;
}

void TetrisServer::RequestDisconnect(SessionKey session_key)
{
	EnqueueSessionTask(session_key, std::make_unique<SessionDisconnectTask>());
}

void TetrisServer::CompleteSessionIO(SessionKey session_key)
{
	auto* session = FindSession(session_key);
	if (session && session->CompleteIO(session_key))
		EnqueueLobbyTask(std::make_unique<LobbyTask>(LobbyTaskType::DISCONNECT, session_key));
}

void TetrisServer::CompleteRoomDisconnect(SessionKey session_key)
{
	auto* session = FindSession(session_key);
	if (session && session->CompleteRoomRemoval(session_key))
		EnqueueLobbyTask(std::make_unique<LobbyTask>(LobbyTaskType::DISCONNECT, session_key));
}

bool TetrisServer::FinalizeDisconnect(SessionKey session_key)
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
	auto* lobby_session = thread_manager_->GetLobbyThreadManager().GetLobbySession(current_session_key.session_index);
	if (lobby_session) lobby_session->Clear(current_session_key);
	session->DiscardTasks();
	session->CompleteTaskProcessing();
	session->FinalizeDisconnect(current_session_key);
	return true;
}

void TetrisServer::StringToCharBuf(const std::string& str, char* buf, int buf_size)
{
	ZeroMemory(buf, buf_size);
	size_t copy_size = std::min(str.size(), static_cast<size_t>(buf_size));
	memcpy(buf, str.data(), copy_size);
}

std::string TetrisServer::CharBufToString(const char* buf, int buf_size)
{
	size_t copy_size = strnlen(buf, static_cast<size_t>(buf_size));
	std::string str(buf, copy_size);
	return str;
}
