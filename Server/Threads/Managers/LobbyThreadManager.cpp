#include "LobbyThreadManager.h"
#include "GameThreadManager.h"
#include "IOCPServer.h"
#include "LobbyPhaseContext.h"
#include "DBTasks.h"
#include "define_packets.h"
#include "packet_types.h"
#include "room_lifecycle_tasks.h"

LobbyThreadManager::LobbyThreadManager(IOCPServer& iocp_server) : iocp_server_(iocp_server)
{
}

void LobbyThreadManager::Start()
{
	thread_objects_.reserve(THREAD_COUNT);
	threads_.reserve(THREAD_COUNT);
	for (int i = 0; i < THREAD_COUNT; ++i) {
		thread_objects_.emplace_back(std::make_unique<LobbyThread>(*this));
		thread_objects_.back()->Start();
		threads_.emplace_back(&ServerThread::Run, thread_objects_.back().get());
	}
}

void LobbyThreadManager::StartLobbyPhase()
{
	if (!iocp_server_.IsRunning()) return;
	for (const auto& lobby_thread : thread_objects_) {
		if (lobby_thread->state_.load() != LobbyThreadState::AVAILABLE) return;
	}

	const auto phase_context = std::make_shared<LobbyPhaseContext>(lifecycle_tasks_.ClaimTaskCount());
	{
		std::lock_guard<std::mutex> lock(lobby_mutex_);
		for (const auto& lobby_thread : thread_objects_) {
			lobby_thread->state_.store(LobbyThreadState::PROCESSING);
			lobby_thread->phase_context_ = phase_context;
			lobby_thread->phase_.store(LobbyPhase::SESSION_PROCESS);
		}
	}
	lobby_cv_.notify_all();
}

void LobbyThreadManager::Enqueue(std::unique_ptr<LobbyTask> task)
{
	lifecycle_tasks_.Enqueue(std::move(task));
}

LobbySession* LobbyThreadManager::GetLobbySession(int session_index)
{
	if (session_index < 0 || session_index >= MAX_PLAYER_COUNT) return nullptr;
	return &lobby_sessions_[session_index];
}

bool LobbyThreadManager::BeginRoomTransition(SessionKey session_key)
{
	auto* session = iocp_server_.FindSession(session_key);
	if (!session || session->GetLifeState() != LifeState::ACTIVE || session->GetModeState() != ModeState::LOBBY) return false;
	auto* lobby_session = GetLobbySession(session_key.session_index);
	return lobby_session && lobby_session->TrySetPending(session_key);
}

void LobbyThreadManager::FindMatch(SessionKey session_key, int max_player_count)
{
	auto* session = iocp_server_.FindSession(session_key);
	if (!session) return;
	if (max_player_count == 0) {
		for (auto& room : iocp_server_.rooms_) {
			// 방에 접근할 때는 무조건 Shared_ptr을 로드해서 참조 카운트를 늘려야 한다. 방이 삭제되더라도 안전하게 동작하기 위해서이다.
			// 단순히 널을 체크하고 들어가도 그 다음 내부 객체 접근 시 그 객체가 삭제되었을 수 있다.
			auto room_sp = room.load();
			if (room_sp) {
				if (room_sp->IsPrivate()) continue;
				if (room_sp->GetMaxPlayerCount() == 2 or room_sp->GetMaxPlayerCount() == 5) { // 공개 멀티 방 중 아무 방이나 찾기
					const RoomInfoSnapshot snapshot = room_sp->GetRoomInfoSnapshot();
					if (snapshot.room_state != RoomState::WAIT || snapshot.current_player_count >= snapshot.max_player_count) continue;
					int result = iocp_server_.game_thread_manager_->TryJoinRoom(session_key, room_sp->GetRoomGen(), "", max_player_count);
					if (result == SUCCESS) return;
					if (result == ErrorCode::INVALID_REQUEST || result == ErrorCode::SERVER_ERROR) {
						iocp_server_.SendError(*session, result);
						return;
					}
				}
			}
		}
	}

	else if (max_player_count == 2) {
		for (auto& room : iocp_server_.rooms_) {
			auto room_sp = room.load();
			if (room_sp) {
				if (room_sp->IsPrivate()) continue;
				if (room_sp->GetMaxPlayerCount() == max_player_count) {
					const RoomInfoSnapshot snapshot = room_sp->GetRoomInfoSnapshot();
					if (snapshot.room_state != RoomState::WAIT || snapshot.current_player_count >= snapshot.max_player_count) continue;
					int result = iocp_server_.game_thread_manager_->TryJoinRoom(session_key, room_sp->GetRoomGen(), "", max_player_count);
					if (result == SUCCESS) return;
					if (result == ErrorCode::INVALID_REQUEST || result == ErrorCode::SERVER_ERROR) {
						iocp_server_.SendError(*session, result);
						return;
					}
				}
			}
		}
	}

	else if (max_player_count == 5) {
		for (auto& room : iocp_server_.rooms_) {
			auto room_sp = room.load();
			if (room_sp) {
				if (room_sp->IsPrivate()) continue;
				if (room_sp->GetMaxPlayerCount() == max_player_count) {
					const RoomInfoSnapshot snapshot = room_sp->GetRoomInfoSnapshot();
					if (snapshot.room_state != RoomState::WAIT || snapshot.current_player_count >= snapshot.max_player_count) continue;
					int result = iocp_server_.game_thread_manager_->TryJoinRoom(session_key, room_sp->GetRoomGen(), "", max_player_count);
					if (result == SUCCESS) return;
					if (result == ErrorCode::INVALID_REQUEST || result == ErrorCode::SERVER_ERROR) {
						iocp_server_.SendError(*session, result);
						return;
					}
				}
			}
		}
	}

	else {
		iocp_server_.SendError(*session, ErrorCode::INVALID_REQUEST);
		return;
	}

	iocp_server_.SendError(*session, ErrorCode::NOT_FOUND_JOINABLE_ROOM);
}

void LobbyThreadManager::BroadcastToLobby(char* packet)
{
	const int packet_size = static_cast<int>(reinterpret_cast<const PACKET_HEADER*>(packet)->size);
	for (const SessionKey session_key : iocp_server_.active_players_.GetActiveSessionKeys()) {
		auto* player = iocp_server_.FindSession(session_key);
		if (player && player->GetModeState() == ModeState::LOBBY) {
			player->SendPacket(packet, packet_size, iocp_server_.iocp_handle_);
		}
	}
}

void LobbyThreadManager::SendRoomList(Session& session)
{
	char packet_buffer[BUF_SIZE];
	int packet_size = 0;
	for (auto& room_sp : iocp_server_.active_rooms_.GetActiveRoomsSnapshot()) {
		if (!room_sp) continue;
		S2C_ROOM_INFO_PACKET info_p;
		RoomInfoSnapshot room_snapshot = room_sp->GetRoomInfoSnapshot();
		if (room_snapshot.room_state != RoomState::WAIT && room_snapshot.room_state != RoomState::PLAY) continue;
		info_p.header.size = static_cast<std::uint16_t>(sizeof(info_p));
		info_p.header.type = S2C_ROOM_INFO;
		info_p.room_gen = room_snapshot.room_gen;
		info_p.max_player_count = room_snapshot.max_player_count;
		info_p.current_player_count = room_snapshot.current_player_count;
		iocp_server_.StringToCharBuf(room_snapshot.room_name, info_p.room_name, sizeof(info_p.room_name));
		info_p.is_private = room_snapshot.is_private;
		info_p.is_play = room_snapshot.room_state == RoomState::PLAY;

		if (packet_size + sizeof(info_p) > BUF_SIZE) {
			session.SendPacket(reinterpret_cast<char*>(packet_buffer), packet_size, iocp_server_.iocp_handle_);
			packet_size = 0;
		}

		memcpy(packet_buffer + packet_size, &info_p, sizeof(info_p));
		packet_size += sizeof(info_p);
		//if (session.GetSessionKey().gen != request_gen) return;
	}
	session.SendPacket(reinterpret_cast<char*>(packet_buffer), packet_size, iocp_server_.iocp_handle_);
}

void LobbyThreadManager::SendLobbyPlayerList(Session& session)
{
	{
		// 비용을 줄이기 위한 선체크
		if (session.GetModeState() != ModeState::LOBBY) return;
	}

	int packet_size = 0;
	char packet_buffer[BUF_SIZE];
	for (const SessionKey session_key : iocp_server_.active_players_.GetActiveSessionKeys()) {
		auto* player = iocp_server_.FindSession(session_key);
		if (!player) continue;
		S2C_LOBBY_PLAYER_INFO_PACKET info_p;
		info_p.header.size = static_cast<std::uint16_t>(sizeof(info_p));
		info_p.header.type = S2C_LOBBY_PLAYER_INFO;
		//info_p.player_id = -1;
		{
			if (player->GetDBInfo().player_id == session.GetDBInfo().player_id) continue;
			if (player->GetModeState() == ModeState::LOBBY) {
				info_p.player_id = player->GetDBInfo().player_id;
				iocp_server_.StringToCharBuf(player->GetDBInfo().nickname, info_p.nickname, MAX_ROOM_NAME_SIZE);
			}
			else continue;
		}

		if (packet_size + sizeof(info_p) > BUF_SIZE) {
			session.SendPacket(reinterpret_cast<char*>(packet_buffer), packet_size, iocp_server_.iocp_handle_);
			packet_size = 0;
		}

		memcpy(packet_buffer + packet_size, &info_p, sizeof(info_p));
		packet_size += sizeof(info_p);
	}
	session.SendPacket(reinterpret_cast<char*>(packet_buffer), packet_size, iocp_server_.iocp_handle_);
}

void LobbyThreadManager::SendFriendList(Session& session)
{
	std::vector<FriendInfo> friend_list;
	{
		if (session.GetModeState() != ModeState::LOBBY) return;
		friend_list = session.GetFriendList();
	}

	int packet_size = 0;
	char packet_buffer[BUF_SIZE];
	for (auto& friend_info : friend_list) {
		S2C_FRIEND_INFO_PACKET info_p; // 얘는 그냥 지 세션에 있는 친구 목록이라 미리 다 작성하고 현재 친구 상태만 검사해서 보내주면 됨
		info_p.header.size = static_cast<std::uint16_t>(sizeof(info_p));
		info_p.header.type = S2C_FRIEND_INFO;
		info_p.player_id = friend_info.player_id;
		info_p.is_lobby = false;
		iocp_server_.StringToCharBuf(friend_info.nickname, info_p.nickname, MAX_PLAYER_NAME_SIZE);

		// 로비인지 체크만 함
		auto* friend_session = iocp_server_.FindSession(iocp_server_.active_players_.FindSessionKeyByID(friend_info.player_id));
		if (friend_session) {
			if (friend_session->GetDBInfo().player_id != friend_info.player_id) continue;
			if (friend_session->GetModeState() == ModeState::LOBBY) info_p.is_lobby = true;
		}

		if (packet_size + sizeof(info_p) > BUF_SIZE) {
			session.SendPacket(reinterpret_cast<char*>(packet_buffer), packet_size, iocp_server_.iocp_handle_);
			packet_size = 0;
		}

		memcpy(packet_buffer + packet_size, &info_p, sizeof(info_p));
		packet_size += sizeof(info_p);
	}
	session.SendPacket(reinterpret_cast<char*>(packet_buffer), packet_size, iocp_server_.iocp_handle_);
}

void LobbyThreadManager::SendRankings(Session& session)
{
	{
		if (session.GetModeState() != ModeState::LOBBY) return;
	}

	std::vector<RankingInfo> rankings = iocp_server_.ranking_manager_.GetRankings();
	if (rankings.empty()) return;

	int packet_size = 0;
	char packet_buffer[BUF_SIZE];
	for (const auto& ranking : rankings) {
		S2C_RANKING_INFO_PACKET info_p{};
		info_p.header.size = static_cast<std::uint16_t>(sizeof(info_p));
		info_p.header.type = S2C_RANKING_INFO;
		iocp_server_.StringToCharBuf(ranking.nickname, info_p.nickname, MAX_PLAYER_NAME_SIZE);
		info_p.score = ranking.score;

		if (packet_size + sizeof(info_p) > BUF_SIZE) {
			session.SendPacket(reinterpret_cast<char*>(packet_buffer), packet_size, iocp_server_.iocp_handle_);
			packet_size = 0;
		}

		memcpy(packet_buffer + packet_size, &info_p, sizeof(info_p));
		packet_size += sizeof(info_p);
	}

	session.SendPacket(reinterpret_cast<char*>(packet_buffer), packet_size, iocp_server_.iocp_handle_);
}

void LobbyThreadManager::ProcessPacket(char* packet, Session& session)
{
	if (!packet) return;
	switch (reinterpret_cast<PACKET_HEADER*>(packet)->type) {
	case C2S_LOGIN: {
		if (session.GetModeState() != ModeState::LOGIN) return;
		C2S_LOGIN_PACKET* recv_p = reinterpret_cast<C2S_LOGIN_PACKET*>(packet);
		std::string login_id = iocp_server_.CharBufToString(recv_p->login_id, sizeof(recv_p->login_id));
		std::string password = iocp_server_.CharBufToString(recv_p->login_password, sizeof(recv_p->login_password));
		iocp_server_.EnqueueDBTask(std::make_unique<DBLoginTask>(session.GetSessionKey(), login_id, password));
		break;
	}
	case C2S_MESSAGE: {
		if (session.GetModeState() != ModeState::LOBBY) return;
		C2S_MESSAGE_PACKET* recv_p = reinterpret_cast<C2S_MESSAGE_PACKET*>(packet);
		int msg_size = recv_p->header.size - sizeof(C2S_MESSAGE_PACKET);
		if (msg_size == 0) return;
		int send_p_size = sizeof(S2C_MESSAGE_PACKET) + msg_size;
		DBResultLogin db_info = session.GetDBInfo();

		char* send_p = new char[send_p_size];
		S2C_MESSAGE_PACKET front_p;
		front_p.header.size = static_cast<std::uint16_t>(send_p_size);
		front_p.header.type = S2C_MESSAGE;
		front_p.player_id = db_info.player_id;
		iocp_server_.StringToCharBuf(db_info.nickname, front_p.nickname, sizeof(front_p.nickname));
		memcpy(send_p, &front_p, sizeof(S2C_MESSAGE_PACKET));
		memcpy(send_p + sizeof(S2C_MESSAGE_PACKET), reinterpret_cast<char*>(recv_p) + sizeof(C2S_MESSAGE_PACKET), msg_size);
		BroadcastToLobby(send_p);
		delete[] send_p;
		break;
	}
	case C2S_TEST: {
		C2S_TEST_PACKET* recv_p = reinterpret_cast<C2S_TEST_PACKET*>(packet);
		char* send_p = new char[recv_p->header.size];
		int msg_size = recv_p->header.size - sizeof(C2S_TEST_PACKET);
		S2C_TEST_PACKET front_p;
		front_p.header.size = recv_p->header.size;
		front_p.header.type = S2C_TEST;
		front_p.player_id = session.GetDBInfo().player_id;
		front_p.last_time = recv_p->last_time;
		memcpy(send_p, &front_p, sizeof(S2C_TEST_PACKET));
		memcpy(send_p + sizeof(S2C_TEST_PACKET), reinterpret_cast<char*>(recv_p) + sizeof(C2S_TEST_PACKET), msg_size);
		BroadcastToLobby(send_p);
		delete[] send_p;
		break;
	}
	case C2S_ADD_PUBLIC_ROOM:
	case C2S_ADD_PRIVATE_ROOM: {
		const SessionKey session_key = session.GetSessionKey();
		if (!BeginRoomTransition(session_key)) break;
		const int packet_size = reinterpret_cast<PACKET_HEADER*>(packet)->size;
		const RoomLifecycleTaskType task_type = reinterpret_cast<PACKET_HEADER*>(packet)->type == C2S_ADD_PUBLIC_ROOM ? RoomLifecycleTaskType::CREATE_PUBLIC : RoomLifecycleTaskType::CREATE_PRIVATE;
		auto task = std::make_unique<RoomLifecycleTask>(task_type, session_key);
		task->packet.assign(packet, packet + packet_size);
		iocp_server_.EnqueueRoomLifecycleTask(std::move(task));
		break;
	}
	case C2S_JOIN_PUBLIC_ROOM: {
		C2S_JOIN_PUBLIC_ROOM_PACKET* join_p = reinterpret_cast<C2S_JOIN_PUBLIC_ROOM_PACKET*>(packet);
		const int result = iocp_server_.game_thread_manager_->TryJoinRoom(session.GetSessionKey(), join_p->room_gen, "");
		if (result != SUCCESS) iocp_server_.SendError(session, result);
		break;
	}
	case C2S_JOIN_PRIVATE_ROOM: {
		C2S_JOIN_PRIVATE_ROOM_PACKET* join_p = reinterpret_cast<C2S_JOIN_PRIVATE_ROOM_PACKET*>(packet);
		const int result = iocp_server_.game_thread_manager_->TryJoinRoom(session.GetSessionKey(), join_p->room_gen, iocp_server_.CharBufToString(join_p->room_password, sizeof(join_p->room_password)));
		if (result != SUCCESS) iocp_server_.SendError(session, result);
		break;
	}
	case C2S_REQUEST_ROOM_LIST:
		SendRoomList(session);
		break;
	case C2S_REQUEST_LOBBY_PLAYER_LIST:
		SendLobbyPlayerList(session);
		break;
	case C2S_REQUEST_FRIEND_LIST:
		SendFriendList(session);
		break;
	case C2S_REQUEST_RANKINGS:
		SendRankings(session);
		break;
	case C2S_FAST_MATCHING: {
		C2S_FAST_MATCHING_PACKET* matching_p = reinterpret_cast<C2S_FAST_MATCHING_PACKET*>(packet);
		FindMatch(session.GetSessionKey(), matching_p->max_player_count);
		break;
	}
	case C2S_ADD_FRIEND_REQUEST: {
		if (session.GetModeState() != ModeState::LOBBY) return;
		C2S_ADD_FRIEND_REQUEST_PACKET* friend_p = reinterpret_cast<C2S_ADD_FRIEND_REQUEST_PACKET*>(packet);
		DBResultLogin db_info = session.GetDBInfo();
		FriendInfo requester_info{ db_info.player_id, db_info.nickname };
		iocp_server_.EnqueueDBTask(std::make_unique<DBAddFriendRequestTask>(session.GetSessionKey(), requester_info, friend_p->receiver_id));
		break;
	}
	case C2S_ACCEPT_FRIEND: {
		if (session.GetModeState() != ModeState::LOBBY) return;
		C2S_ACCEPT_FRIEND_PACKET* accept_p = reinterpret_cast<C2S_ACCEPT_FRIEND_PACKET*>(packet);
		DBResultLogin db_info = session.GetDBInfo();
		FriendInfo acceptor_info{ db_info.player_id, db_info.nickname };
		iocp_server_.EnqueueDBTask(std::make_unique<DBAddFriendTask>(session.GetSessionKey(), acceptor_info, accept_p->requester_id));
		break;
	}
	case C2S_DELETE_FRIEND: {
		if (session.GetModeState() != ModeState::LOBBY) return;
		C2S_DELETE_FRIEND_PACKET* delete_p = reinterpret_cast<C2S_DELETE_FRIEND_PACKET*>(packet);
		iocp_server_.EnqueueDBTask(std::make_unique<DBDeleteFriendTask>(session.GetSessionKey(), delete_p->target_id));
		break;
	}
	default:
		break;
	}
}

void LobbyThreadManager::ProcessTask(std::unique_ptr<LobbyTask> task)
{
	if (!task) return;
	auto* lobby_session = GetLobbySession(task->session_key.session_index);
	switch (task->task_type) {
	case LobbyTaskType::ADD_SESSION: {
		auto* session = iocp_server_.FindSession(task->session_key);
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
		auto* session = iocp_server_.FindSession(task->session_key);
		if (!session) {
			lobby_session->Clear(task->session_key);
			return;
		}
		if (!lobby_session->Activate(task->session_key)) return;
		if (task->matching_max_player_count >= 0) FindMatch(task->session_key, task->matching_max_player_count);
		else iocp_server_.SendError(*session, task->result);
		break;
	}
	case LobbyTaskType::ENTER_LOBBY: {
		int result = ErrorCode::INVALID_REQUEST;
		auto* session = iocp_server_.FindSession(task->session_key);
		bool is_processing = false;
		if (lobby_session && session && session->GetLifeState() == LifeState::ACTIVE) {
			if (!session->TryStartTaskProcessing()) {
				Enqueue(std::move(task));
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
		iocp_server_.EnqueueRoomLifecycleTask(std::move(room_task));
		if (result == SUCCESS) lobby_session->Activate(task->session_key);
		if (is_processing) session->CompleteTaskProcessing();
		break;
	}
	case LobbyTaskType::DISCONNECT:
		if (!iocp_server_.FinalizeDisconnect(task->session_key)) Enqueue(std::move(task));
		break;
	}
}

void LobbyThreadManager::Close()
{
	for (auto& thread_object : thread_objects_)
		thread_object->Close();
}

void LobbyThreadManager::Join()
{
	for (auto& thread : threads_)
		thread.join();
}
