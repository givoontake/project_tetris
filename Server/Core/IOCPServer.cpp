#include <iostream>
#include <algorithm>
#include "IOCPServer.h"
#include <Windows.h>
#include <bcrypt.h>
#include "DBThreadManager.h"
#include "LobbyThreadManager.h"
#include "GameThreadManager.h"
#include "lobby_tasks.h"
#include "room_lifecycle_tasks.h"
#include "session_tasks.h"

#undef min

#pragma comment(lib, "bcrypt.lib")

IOCPServer::IOCPServer()
	: db_result_handler_(*this)
{
	//for (int i = 0; i < MAX_ROOM_COUNT; ++i) {

	//	auto room = rooms_[i].load();
	//	room = nullptr;
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


void IOCPServer::SendError(Session& session, int error_code)
{
	S2C_ERROR_PACKET error_p;
	error_p.header.size = static_cast<std::uint16_t>(sizeof(error_p));
	error_p.header.type = S2C_ERROR;
	error_p.error_code = error_code;

	session.SendPacket(reinterpret_cast<char*>(&error_p), error_p.header.size, iocp_handle_);
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

bool IOCPServer::ProcessRecvBuffer(Session& session, SessionKey session_key, int recv_bytes)
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

SessionTaskProcessResult IOCPServer::ProcessSessionTask(Session& session, std::unique_ptr<SessionTask> task)
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

void IOCPServer::RoutePacket(char* packet, Session& session)
{
	PrintPacketType(reinterpret_cast<PACKET_HEADER*>(packet)->type);
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
		lobby_thread_manager_->ProcessPacket(packet, session);
		break;
	case ModeState::ROOM:
		game_thread_manager_->ProcessPacket(packet, session);
		break;
	}

}

//void IOCPServer::SendToSelf(char* packet, int self_index)
//{
//	sessions_[self_index]->SendPacket(packet, reinterpret_cast<PACKET_HEADER*>(packet)->size, iocp_handle_);
//}

void IOCPServer::RequestLoadRankings()
{
	EnqueueDBTask(std::make_unique<DBLoadRankingsTask>());
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

