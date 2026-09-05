#include "MultiRoom.h"
#include "common_packets.h"
#include "game_packets.h"
#include "packet_types.h"
#include "room_packets.h"

MultiRoom::MultiRoom(TetrisServer* server, PublicRoomInitData data)
	: TetrisRoom(server, data)
{
}

MultiRoom::MultiRoom(TetrisServer* server, PrivateRoomInitData data)
	: TetrisRoom(server, data)
{
}

MultiRoom::~MultiRoom()
{
}

bool MultiRoom::AddHostSession(Session& session, SessionKey session_key)
{
	if (!InitHostSession(session, session_key)) return false;
	host_id_ = session.GetDBInfo().player_id;
	return true;
}

void MultiRoom::HandlePacket(char* packet, Session& request_session)
{
	switch (reinterpret_cast<PACKET_HEADER*>(packet)->type) {
	case C2S_READY: {
		RoomTask task;
		task.task_type = RoomTaskType::READY;
		task.session_key = request_session.GetSessionKey();
		AddRoomTask(std::move(task));
		break;
	}
	case C2S_KICK: {
		C2S_KICK_PACKET* kick_p = reinterpret_cast<C2S_KICK_PACKET*>(packet);
		RoomTask task;
		task.task_type = RoomTaskType::KICK;
		task.session_key = request_session.GetSessionKey();
		task.target_player_id = kick_p->kick_player_id;
		AddRoomTask(std::move(task));
		break;
	}
	default:
		TetrisRoom::HandlePacket(packet, request_session);
		break;
	}
}

int MultiRoom::AddPlayer(Session& new_session, SessionKey session_key, const std::string& room_password)
{
	if (new_session.GetLifeState() != LifeState::ACTIVE || new_session.GetModeState() != ModeState::LOBBY) return ErrorCode::INVALID_REQUEST;
	if (room_password_ != room_password) return ErrorCode::ROOM_INVALID_PASSWORD;
	const RoomState room_state = room_state_.load();
	if (room_state == RoomState::PLAY) return ErrorCode::ROOM_IN_GAME;
	if (room_state != RoomState::WAIT) return ErrorCode::ROOM_NOT_FOUND;
	auto room_players = GetRoomPlayers();
	if (FindPlayer(session_key)) return ErrorCode::INVALID_REQUEST;
	int result = ErrorCode::ROOM_FULL;
	int added_slot = -1;
	for (int i = 0; i < room_players.size(); ++i) {
		auto& room_player = room_players[i];
		if (room_player.HasSession()) continue;
		if (!room_player.InitPlayer(new_session, session_key, room_index_)) return ErrorCode::INVALID_REQUEST;
		++current_player_count_;
		result = SUCCESS;
		added_slot = i;
		break;
	}

	if (result == SUCCESS) {
		if (room_password_.empty()) {
			S2C_ADD_PUBLIC_ROOM_PACKET public_p;
			public_p.header.size = static_cast<std::uint16_t>(sizeof(public_p));
			public_p.header.type = S2C_ADD_PUBLIC_ROOM;
			public_p.room_gen = room_gen_;
			public_p.max_player_count = max_player_count_;
			server_->StringToCharBuf(room_name_, public_p.room_name, sizeof(public_p.room_name));
			new_session.SendPacket(reinterpret_cast<char*>(&public_p), public_p.header.size, server_->GetIOCPHandle());
		}
		else {
			S2C_ADD_PRIVATE_ROOM_PACKET private_p;
			private_p.header.size = static_cast<std::uint16_t>(sizeof(private_p));
			private_p.header.type = S2C_ADD_PRIVATE_ROOM;
			private_p.room_gen = room_gen_;
			private_p.max_player_count = max_player_count_;
			server_->StringToCharBuf(room_name_, private_p.room_name, sizeof(private_p.room_name));
			server_->StringToCharBuf(room_password_, private_p.room_password, sizeof(private_p.room_password));
			new_session.SendPacket(reinterpret_cast<char*>(&private_p), private_p.header.size, server_->GetIOCPHandle());
		}

		// 본인의 입장을 본인 제외 나머지에게(방 생성 시 본인은 방에 추가된다)
		for (int i = 0; i < room_players.size(); ++i) {
			if (i == added_slot) continue;
			auto& room_player = room_players[i];
			auto session = FindSession(room_player);
			if (!session) continue;
			S2C_ADD_PLAYER_PACKET add_p;
			add_p.header.size = static_cast<std::uint16_t>(sizeof(add_p));
			add_p.header.type = S2C_ADD_PLAYER;
			add_p.player_id = new_session.GetDBInfo().player_id;
			server_->StringToCharBuf(new_session.GetDBInfo().nickname, add_p.nickname, sizeof(add_p.nickname));
			session->SendPacket(reinterpret_cast<char*>(&add_p), add_p.header.size, server_->GetIOCPHandle());
		}

		// 본인 제외 나머지 플레이어를 본인에게
		for (int i = 0; i < room_players.size(); ++i)	{
			if (i == added_slot) continue;
			auto& room_player = room_players[i];
			auto session = FindSession(room_player);
			if (!session) continue;
			S2C_ADD_PLAYER_PACKET add_p;
			add_p.header.size = static_cast<std::uint16_t>(sizeof(add_p));
			add_p.header.type = S2C_ADD_PLAYER;
			add_p.player_id = session->GetDBInfo().player_id;
			server_->StringToCharBuf(session->GetDBInfo().nickname, add_p.nickname, sizeof(add_p.nickname));
			new_session.SendPacket(reinterpret_cast<char*>(&add_p), add_p.header.size, server_->GetIOCPHandle());
		}

		// 새로 입장한 세션에게 방장이 누구인지
		S2C_UPDATE_HOST_PACKET host_p;
		host_p.header.size = static_cast<std::uint16_t>(sizeof(host_p));
		host_p.header.type = S2C_UPDATE_HOST;
		host_p.new_host_id = host_id_;
		new_session.SendPacket(reinterpret_cast<char*>(&host_p), host_p.header.size, server_->GetIOCPHandle());
		return result;
	}

	return result;
}

void MultiRoom::RemovePlayer(SessionKey session_key)
{
	auto* session = server_->FindSession(session_key);
	if (session && session->GetLifeState() == LifeState::ACTIVE) {
		RequestLobbyTransition(session_key);
		return;
	}
	CompletePlayerRemoval(session_key, RoomExitType::LEAVE);
}

void MultiRoom::CompletePlayerRemoval(SessionKey session_key, RoomExitType exit_type)
{
	auto room_players = GetRoomPlayers();
	for (auto& room_player : room_players) {
		if (room_player.MatchesSessionKey(session_key)) { // 삭제할 세션 검색
			const int player_id = room_player.GetSessionKey().player_id;
			if (current_player_count_.load() == 1) BeginRoomDelete();
			S2C_REMOVE_PLAYER_PACKET p;
			p.header.size = static_cast<std::uint16_t>(sizeof(p));
			p.header.type = S2C_REMOVE_PLAYER;
			p.player_id = player_id;

			Broadcast(reinterpret_cast<char*>(&p), server_->GetIOCPHandle());
			if (exit_type == RoomExitType::KICK) {
				auto* session = server_->FindSession(session_key);
				if (session) {
					S2C_INFO_PACKET info_p;
					info_p.header.size = static_cast<std::uint16_t>(sizeof(info_p));
					info_p.header.type = S2C_INFO;
					info_p.info_code = InfoCode::KICKED;
					session->SendPacket(reinterpret_cast<char*>(&info_p), info_p.header.size, server_->GetIOCPHandle());
				}
			}
			const SessionKey disconnected_session_key = ClearPlayer(room_player);
			--current_player_count_;
			if (player_id == host_id_) FindNewHost(); // 여기서 찾기 및 못찾을 경우 삭제까지 같이 함
			if (disconnected_session_key.session_index >= 0) server_->CompleteRoomDisconnect(disconnected_session_key);
			break;
		}
	}
}

void MultiRoom::HandlePlayerReactivated()
{
	if (host_id_ < 0) FindNewHost();
}

void MultiRoom::SendCreateRoom(Session& session)
{
	if (room_password_.empty()) {
		S2C_ADD_PUBLIC_ROOM_PACKET public_p;
		public_p.header.size = static_cast<std::uint16_t>(sizeof(public_p));
		public_p.header.type = S2C_ADD_PUBLIC_ROOM;
		public_p.room_gen = room_gen_;
		public_p.max_player_count = max_player_count_;
		server_->StringToCharBuf(room_name_, public_p.room_name, sizeof(public_p.room_name));
		session.SendPacket(reinterpret_cast<char*>(&public_p), public_p.header.size, server_->GetIOCPHandle());
	}
	else {
		S2C_ADD_PRIVATE_ROOM_PACKET private_p;
		private_p.header.size = static_cast<std::uint16_t>(sizeof(private_p));
		private_p.header.type = S2C_ADD_PRIVATE_ROOM;
		private_p.room_gen = room_gen_;
		private_p.max_player_count = max_player_count_;
		server_->StringToCharBuf(room_name_, private_p.room_name, sizeof(private_p.room_name));
		server_->StringToCharBuf(room_password_, private_p.room_password, sizeof(private_p.room_password));
		session.SendPacket(reinterpret_cast<char*>(&private_p), private_p.header.size, server_->GetIOCPHandle());
	}
}

void MultiRoom::TogglePlayerReady(int player_id)
{
	if (host_id_ == player_id) return;
	auto room_players = GetRoomPlayers();
	bool is_ready = false;
	for (auto& room_player : room_players) {
		auto session = FindSession(room_player);
		if (!session) continue;
		if (session->GetDBInfo().player_id == player_id) { // 레디 상태
			if (room_player.GetRoomPlayerState() == RoomPlayerState::READY) {
				room_player.SetRoomPlayerState(RoomPlayerState::WAIT);
				is_ready = false;
			}
			else if (room_player.GetRoomPlayerState() == RoomPlayerState::WAIT) {
				room_player.SetRoomPlayerState(RoomPlayerState::READY);
				is_ready = true;
			}
			S2C_READY_PACKET p;
			p.header.size = static_cast<std::uint16_t>(sizeof(p));
			p.header.type = S2C_READY;
			p.player_id = player_id;
			p.is_ready = is_ready;

			Broadcast(reinterpret_cast<char*>(&p), server_->GetIOCPHandle());
			break;
		}
	}
}

bool MultiRoom::KickPlayer(int requester_id, int kick_player_id)
{
	if (requester_id != host_id_) return true;
	if (kick_player_id == host_id_) return true;
	auto room_players = GetRoomPlayers();

	for (auto& room_player : room_players) {
		if (room_player.GetSessionKey().player_id != kick_player_id) continue;
		auto session = FindSession(room_player);
		if (!session) continue;
		if (!session->TryStartTaskProcessing()) return false;
		if (!session->MatchesSessionKey(room_player.GetSessionKey()) || !IsPlayerInRoom(*session)) {
			session->CompleteTaskProcessing();
			return true;
		}

		RequestLobbyTransition(room_player.GetSessionKey(), RoomExitType::KICK);
		session->CompleteTaskProcessing();
		return true;
	}
	return true;
}

void MultiRoom::StartGame(int requester_id)
{
	if (requester_id != host_id_) return;
	if (room_state_.load() == RoomState::PLAY) return;
	auto room_players = GetRoomPlayers();

	int ready_player_count = 0;
	int result = -1;
	for (auto& room_player : room_players) {
		auto session = FindSession(room_player);
		if (!session) continue; // 사용 중이지 않은 인덱스는 건너뜀
		if (session->GetDBInfo().player_id == host_id_) ++ready_player_count;

		else if (room_player.GetRoomPlayerState() == RoomPlayerState::READY) ++ready_player_count;
	}

	int current_player_count = GetCurrentPlayerCount();
	if (ready_player_count == current_player_count) {
		if (current_player_count == 1) {
			result = ErrorCode::ROOM_NOT_ENOUGH_PLAYERS;
		}
		else {
			result = SUCCESS;
		}
	}
	else result = ErrorCode::ROOM_NOT_ALL_READY;

	if (result != SUCCESS) {
		S2C_ERROR_PACKET error_p;
		error_p.header.size = static_cast<std::uint16_t>(sizeof(error_p));
		error_p.header.type = S2C_ERROR;
		error_p.error_code = result;
		int host_index = FindHostIndex(host_id_);
		if (host_index >= 0) {
			auto host_session = FindSession(room_players[host_index]);
			if (host_session) host_session->SendPacket(reinterpret_cast<char*>(&error_p), error_p.header.size, server_->GetIOCPHandle());
		}
		return;
	}

	if(!TryChangeRoomState(RoomState::WAIT, RoomState::PLAY)) return; // 잘못된 요청(동시 요청 등)에 대한 방어 코드 -> CAS에 성공해야만 시작
	play_generation_.fetch_add(1);
	
	S2C_MULTI_START_PACKET start_p;
	start_p.header.size = static_cast<std::uint16_t>(sizeof(start_p));
	start_p.header.type = S2C_MULTI_START;
	Broadcast(reinterpret_cast<char*>(&start_p), server_->GetIOCPHandle());
	// 모든 조건 통과->게임 시작
	AppendTetromino7Bag();
	for (auto& room_player : room_players) {
		auto session = FindSession(room_player);
		if (!session) continue;
		room_player.SetRoomPlayerState(RoomPlayerState::PLAY);
		room_player.GetTetris().InitNewTetromino(tetromino_spawn_list_[room_player.GetTetrominoIndex()], spawn_pos_);
	}

	// 테트리스 게임 중에 들어오는 패킷은 또 따로 분리하고 싶기는 한데..
	InitGame();

	for (auto& room_player : room_players) {
		auto session = FindSession(room_player);
		if (!session) continue;
		S2C_SPAWN_PACKET spawn_p;
		spawn_p.header.size = static_cast<std::uint16_t>(sizeof(spawn_p));
		spawn_p.header.type = S2C_SPAWN;
		spawn_p.player_id = session->GetDBInfo().player_id;
		spawn_p.tetromino_type = tetromino_spawn_list_[room_player.GetTetrominoIndex()];
		spawn_p.next_tetromino_type = tetromino_spawn_list_[room_player.GetTetrominoIndex() + 1];
		spawn_p.spawn_x = spawn_pos_.x;
		spawn_p.spawn_y = spawn_pos_.y;
		Broadcast(reinterpret_cast<char*>(&spawn_p), server_->GetIOCPHandle());
	}
}

bool MultiRoom::ProcessSpecificRoomTask(const RoomTask& task)
{
	auto session = server_->FindSession(task.session_key);
	if (!session) return true;
	switch (task.task_type) {
	case RoomTaskType::READY:
		if (IsPlayerInRoom(*session)) TogglePlayerReady(session->GetDBInfo().player_id);
		break;
	case RoomTaskType::KICK:
		if (IsPlayerInRoom(*session)) return KickPlayer(session->GetDBInfo().player_id, task.target_player_id);
		break;
	case RoomTaskType::START:
		if (IsPlayerInRoom(*session)) StartGame(session->GetDBInfo().player_id);
		break;
	default:
		break;
	}
	return true;
}

void MultiRoom::ProcessGameTick(long long tick_time_ms)
{
	UpdatePrevPlayerStates();

	auto room_players = GetRoomPlayers();
	for (auto& room_player : room_players) {
		auto session = FindSession(room_player);
		if (!session) continue;
		room_player.GetTetris().ProcessTick(tick_time_ms);
	}

	DistributeGarbageLines();
	AddGarbageLines();
	// 승자 나왔으면 종료 아니면 계속 진행해야되니 스폰 체크
	bool is_game_end = false;
	is_game_end = ResolveWinner();
	if (!is_game_end) AddSpawnTasks();
	SessionKey failed_session_key = AppendTickPackets();
	if (failed_session_key.session_index != -1) {
		RoomTask task;
		task.task_type = RoomTaskType::REMOVE_PLAYER;
		task.session_key = failed_session_key;
		AddRoomTask(std::move(task));
		return;
	}

	ResetPlayerTickState();
	BroadcastPackets();
	if (is_game_end) {
		RequestUpdateMatchResult();
		ClearGame();
	}
}

bool MultiRoom::ResolveWinner()
{
	auto room_players = GetRoomPlayers();
	int over_count = 0;
	int player_count = 0;
	for (auto& room_player : room_players) {
		if (room_player.GetRoomPlayerState() != RoomPlayerState::PLAY) continue;
		auto session = FindSession(room_player);
		if (!session) continue;
		++player_count;
	}

	int new_winner_id = -1;
	if (player_count == 1) {
		for (auto& room_player : room_players) {
			if (room_player.GetRoomPlayerState() == RoomPlayerState::PLAY) {
				auto session = FindSession(room_player);
				if (!session) continue;
				new_winner_id = session->GetDBInfo().player_id;
				break;
			}
		}
	}

	else if (player_count == 0) { // 동시에 게임오버된 상태 -> 결국 승자는 정해줘야함.
		for (auto& room_player : room_players) {
			if ((room_player.GetRoomPlayerState() == RoomPlayerState::GAME_OVER) || (room_player.GetPrevRoomPlayerState() == RoomPlayerState::PLAY)) {
				auto session = FindSession(room_player);
				if (!session) continue;
				new_winner_id = session->GetDBInfo().player_id; // 컨테이너 앞쪽에 있는 사람이 승자
				break;
			}
		}
	}

	else return false;
	if (new_winner_id == -1) return false;
	winner_id_ = new_winner_id;

	for (auto& room_player : room_players) {
		auto session = FindSession(room_player);
		if (!session) continue;
		TaskType task;
		task.event_type = EventType::GAME_END;
		task.task = TaskGameEnd{ winner_id_ };
		room_player.GetTetris().GetSendTasks().emplace_back(task);
	}

	return true;
}

void MultiRoom::DistributeGarbageLines()
{	
	// 서로 클리어한 라인을 통해 남에게 패널티를 부여할 라인을 모두 계산한 뒤, 한 번에 적용
	auto room_players = GetRoomPlayers();
	for (int i = 0; i < room_players.size(); ++i) {
		if (room_players[i].IsActive() && room_players[i].GetRoomPlayerState() == RoomPlayerState::PLAY) {
			int garbage_line_count = CalculateGarbageLineCount(room_players[i].GetTetris().GetClearedLineCount());
			if (garbage_line_count > 0) {
				for (int j = 0; j < room_players.size(); ++j) {
					if (room_players[j].IsActive() && room_players[j].GetRoomPlayerState() == RoomPlayerState::PLAY && i != j) {
						(room_players[j].GetTetris().AddPendingGarbageLines(garbage_line_count));
					}
				}
			}
		}
	}
}

int MultiRoom::CalculateGarbageLineCount(int cleared_line_count)
{
	int garbage_lines = 0;
	// 지워진 라인에 따라 증가되는 라인 수가 다름
	switch (cleared_line_count) {
	case 1:
		garbage_lines = 0;
		break;

	case 2:
		garbage_lines = 1;
		break;

	case 3:
		garbage_lines = 2;
		break;

	case 4:
		garbage_lines = 4;
		break;

	default:
		garbage_lines = 0;
	}
	return garbage_lines;
}

void MultiRoom::UpdatePrevPlayerStates()
{
	if (room_state_ != RoomState::PLAY) return;
	auto room_players = GetRoomPlayers();
	for (auto& room_player : room_players) {
		auto session = FindSession(room_player);
		if (!session) continue;
		room_player.SetPrevRoomPlayerState(room_player.GetRoomPlayerState());
	}
}

void MultiRoom::RequestUpdateMatchResult()
{
	SessionKey winner_key{};
	winner_key.player_id = winner_id_;
	SessionKey player_keys[MAX_MATCH_RESULT_PLAYERS]{};
	uint8_t player_count = 0;

	auto room_players = GetRoomPlayers();
	for (auto& room_player : room_players) {
		auto session = FindSession(room_player);
		if (!session) continue;
		player_keys[player_count] = session->GetSessionKey();
		if (player_keys[player_count].player_id == winner_id_) winner_key = player_keys[player_count];
		++player_count;
	}

	server_->EnqueueDBTask(std::make_unique<DBUpdateMatchResultTask>(winner_key, player_keys, player_count));
}

void MultiRoom::FindNewHost()
{
	bool has_found_host = false;
	auto room_players = GetRoomPlayers();
	
	if (GetCurrentPlayerCount() != 0) {
		for (auto& room_player : room_players) {
			auto session = FindSession(room_player);
			if (!session) continue;
			host_id_ = session->GetDBInfo().player_id;
			if (room_state_ == RoomState::WAIT) room_player.SetRoomPlayerState(RoomPlayerState::WAIT); // 게임 중이 아닐 때, 호스트가 나갔을 때 레디 상태인 사람이 호스트가 되면, 레디 상태를 풀어줘야함
			has_found_host = true;
			break;
		}
	}

	if (has_found_host) {
		S2C_UPDATE_HOST_PACKET host_p;
		host_p.header.size = static_cast<std::uint16_t>(sizeof(host_p));
		host_p.header.type = S2C_UPDATE_HOST;
		host_p.new_host_id = host_id_;
		Broadcast(reinterpret_cast<char*>(&host_p), server_->GetIOCPHandle());
	}

	else if (GetCurrentPlayerCount() == 0) {
		BeginRoomDelete();
	}
	else host_id_ = -1;
}

int MultiRoom::FindHostIndex(int host_id)
{
	auto room_players = GetRoomPlayers();
	for (int i = 0; i < room_players.size(); ++i) {
		auto session = FindSession(room_players[i]);
		if (!session) continue;
		if (session->GetDBInfo().player_id == host_id) return i;
	}
	return -1;
}
