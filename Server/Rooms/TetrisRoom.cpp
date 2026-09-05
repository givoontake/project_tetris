#include <random>
#include <utility>
#include "TetrisRoom.h"
#include "IOCPServer.h"
#include "packet_types.h"
#include "lobby_tasks.h"
#include "room_lifecycle_tasks.h"

TetrisRoom::TetrisRoom(IOCPServer* server, PublicRoomInitData data)
{
	// 생성과 소멸은 스레드 세이프하지는 않지만, 어차피 make_shared하고 CAS해서 룸 리스트에 할당하기 전에는 접근되지 않는다.
	server_ = server;
	max_player_count_ = data.max_player_count;
	room_name_ = std::move(data.room_name);
	room_password_.clear();
	room_index_ = data.room_index;
	room_gen_ = data.room_gen;
	room_state_.store(RoomState::EMPTY);

	//SendAddRoom(session);
	current_player_count_.store(0);
}

TetrisRoom::TetrisRoom(IOCPServer* server, PrivateRoomInitData data)
{
	server_ = server;
	max_player_count_ = data.max_player_count;
	room_name_ = std::move(data.room_name);
	room_password_ = std::move(data.room_password);
	room_index_ = data.room_index;
	room_gen_ = data.room_gen;
	room_state_.store(RoomState::EMPTY);
	//SendAddRoom(session);
	current_player_count_.store(0);
}

TetrisRoom::~TetrisRoom()
{
}

int TetrisRoom::GetCurrentPlayerCount() const
{
	return static_cast<int>(current_player_count_.load());
}

bool TetrisRoom::InitHostSession(Session& session, SessionKey session_key)
{
	auto room_players = GetRoomPlayers();
	if (room_players.empty()) return false;
	if (room_players[0].HasSession()) return false;
	if (!room_players[0].InitPlayer(session, session_key, room_index_)) return false;
	current_player_count_.store(1);
	return true;
}

Session* TetrisRoom::FindSession(const Player& player) const
{
	if (!player.IsActive()) return nullptr;
	return server_->FindSession(player.GetSessionKey());
}

Player* TetrisRoom::FindPlayer(SessionKey session_key)
{
	auto room_players = GetRoomPlayers();
	for (auto& room_player : room_players) {
		if (room_player.MatchesSessionKey(session_key)) return &room_player;
	}
	return nullptr;
}

SessionKey TetrisRoom::ClearPlayer(Player& player)
{
	const SessionKey session_key = player.GetSessionKey();
	auto* session = server_->FindSession(session_key);
	player.ClearPlayer();
	if (!session) return {};
	const LifeState life_state = session->GetLifeState();
	return life_state == LifeState::DISCONNECT_PENDING || life_state == LifeState::DISCONNECTING ? session_key : SessionKey{};
}

bool TetrisRoom::RequestLobbyTransition(SessionKey session_key, RoomExitType exit_type)
{
	auto* player = FindPlayer(session_key);
	if (!player || !player->TrySetPending(session_key)) return false;
	auto task = std::make_unique<LobbyTask>(LobbyTaskType::ENTER_LOBBY, session_key);
	task->exit_type = exit_type;
	task->room_index = room_index_;
	task->room_gen = room_gen_;
	server_->EnqueueLobbyTask(std::move(task));
	return true;
}

bool TetrisRoom::ActivatePlayer(SessionKey session_key)
{
	auto* player = FindPlayer(session_key);
	return player && player->Activate(session_key);
}

void TetrisRoom::CompleteLobbyTransition(SessionKey session_key, int result, RoomExitType exit_type)
{
	auto* player = FindPlayer(session_key);
	if (!player || player->GetActiveState() != ActiveEntryState::PENDING) return;
	if (result == SUCCESS) {
		CompletePlayerRemoval(session_key, exit_type);
		return;
	}

	auto* session = server_->FindSession(session_key);
	if (session) {
		const RoomSnapshot room_snapshot = session->GetRoomSnapshot();
		if (session->GetLifeState() == LifeState::ACTIVE && room_snapshot.mode_state == ModeState::ROOM && room_snapshot.room_index == room_index_) {
			if (player->Activate(session_key)) HandlePlayerReactivated();
			return;
		}
	}
	CompletePlayerRemoval(session_key, exit_type);
}

bool TetrisRoom::AddHostSession(Session& session, SessionKey session_key)
{
	return InitHostSession(session, session_key);
}

RoomInfoSnapshot TetrisRoom::GetRoomInfoSnapshot()
{
	RoomInfoSnapshot snapshot;
	snapshot.room_gen = room_gen_;
	snapshot.max_player_count = static_cast<int>(max_player_count_);
	snapshot.current_player_count = static_cast<int>(current_player_count_.load());
	snapshot.room_name = room_name_;
	snapshot.is_private = !room_password_.empty();
	snapshot.room_state = room_state_.load();
	return snapshot;
}

void TetrisRoom::ClearPlayTasks()
{
	const std::size_t task_count = play_tasks_.ClaimTaskCount();
	for (std::size_t i = 0; i < task_count; ++i) play_tasks_.Dequeue();
}

bool TetrisRoom::IsPlayerInRoom(const Session& session) const
{
	const RoomSnapshot snapshot = session.GetRoomSnapshot();
	return snapshot.mode_state == ModeState::ROOM && snapshot.room_index == room_index_;
}

void TetrisRoom::HandlePacket(char* packet, Session& request_session)
{
	switch (reinterpret_cast<PACKET_HEADER*>(packet)->type) {
	case C2S_REMOVE_PLAYER: {
		RequestLobbyTransition(request_session.GetSessionKey());
		break;
	}
	case C2S_START: {
		RoomTask task;
		task.task_type = RoomTaskType::START;
		task.session_key = request_session.GetSessionKey();
		AddRoomTask(std::move(task));
		break;
	}
	case C2S_MOVE: {
		if (!IsPlayerInRoom(request_session)) return;
		const std::uint64_t current_play_generation = play_generation_.load();
		if (room_state_.load() != RoomState::PLAY) return;
		C2S_MOVE_PACKET* recv_p = reinterpret_cast<C2S_MOVE_PACKET*>(packet);
		PlayerInputTask task;
		task.player_id = request_session.GetDBInfo().player_id;
		task.event_type = static_cast<EventType>(recv_p->move_type);
		task.play_generation = current_play_generation;
		AddPlayTask(std::move(task));
		break;
	}
	default:
		break;
	}
}

bool TetrisRoom::AddRoomTask(RoomTask task)
{
	RoomState state = room_state_.load();
	if (state != RoomState::WAIT && state != RoomState::PLAY) return false;
	room_tasks_.Enqueue(std::move(task));
	state = room_state_.load();
	return state != RoomState::DELETE_POST;
}

void TetrisRoom::AddPlayTask(PlayerInputTask task)
{
	play_tasks_.Enqueue(std::move(task));
}

void TetrisRoom::ProcessSessionTasks()
{
	auto room_players = GetRoomPlayers();
	for (auto& room_player : room_players) {
		if (!room_player.IsActive()) continue;
		const SessionKey session_key = room_player.GetSessionKey();
		auto* session = server_->FindSession(session_key);
		if (!session || session->GetLifeState() != LifeState::ACTIVE) continue;
		RoomSnapshot room_snapshot = session->GetRoomSnapshot();
		if (room_snapshot.mode_state != ModeState::ROOM || room_snapshot.room_index != room_index_) continue;
		if (!session->TryStartTaskProcessing()) continue;

		room_snapshot = session->GetRoomSnapshot();
		if (!room_player.IsActive() || !room_player.MatchesSessionKey(session_key) || !session->MatchesSessionKey(session_key) || session->GetLifeState() != LifeState::ACTIVE || room_snapshot.mode_state != ModeState::ROOM || room_snapshot.room_index != room_index_) {
			session->CompleteTaskProcessing();
			continue;
		}

		const std::size_t task_count = session->ClaimTaskCount();
		for (std::size_t i = 0; i < task_count; ++i) {
			const SessionTaskProcessResult result = server_->ProcessSessionTask(*session, session->DequeueTask());
			if (result == SessionTaskProcessResult::DISCARD_REMAINING) {
				for (++i; i < task_count; ++i) session->DequeueTask();
				session->DiscardTasks();
				break;
			}

			room_snapshot = session->GetRoomSnapshot();
			if (result == SessionTaskProcessResult::RESTORE_REMAINING || !room_player.IsActive() || !room_player.MatchesSessionKey(session_key) || session->GetLifeState() != LifeState::ACTIVE || room_snapshot.mode_state != ModeState::ROOM || room_snapshot.room_index != room_index_) {
				session->RestoreClaimedTaskCount(task_count - i - 1);
				break;
			}
		}
		session->CompleteTaskProcessing();
	}
}

bool TetrisRoom::TryProcessRoomTask(const RoomTask& task)
{
	auto* session = server_->FindSession(task.session_key);
	if (!session) {
		if (task.task_type == RoomTaskType::REMOVE_PLAYER) RemovePlayer(task.session_key);
		return true;
	}
	if (!session->TryStartTaskProcessing()) return false;
	if (!session->MatchesSessionKey(task.session_key)) {
		session->CompleteTaskProcessing();
		return true;
	}
	if (task.task_type == RoomTaskType::REMOVE_PLAYER) {
		RemovePlayer(task.session_key);
		session->CompleteTaskProcessing();
		return true;
	}
	auto* player = FindPlayer(task.session_key);
	if (!player || !player->IsActive()) {
		session->CompleteTaskProcessing();
		return true;
	}
	const RoomSnapshot room_snapshot = session->GetRoomSnapshot();
	if (session->GetLifeState() != LifeState::ACTIVE || room_snapshot.mode_state != ModeState::ROOM || room_snapshot.room_index != room_index_) {
		session->CompleteTaskProcessing();
		return true;
	}
	const bool is_processed = ProcessSpecificRoomTask(task);
	session->CompleteTaskProcessing();
	return is_processed;
}

void TetrisRoom::ProcessRoomTasks()
{
	if (pending_room_task_) {
		if (!TryProcessRoomTask(*pending_room_task_)) return;
		pending_room_task_.reset();
	}

	const std::size_t task_count = room_tasks_.ClaimTaskCount();
	for (std::size_t i = 0; i < task_count; ++i) {
		RoomTask task = room_tasks_.Dequeue();
		if (TryProcessRoomTask(task)) continue;
		pending_room_task_ = std::move(task);
		room_tasks_.RestoreClaimedTaskCount(task_count - i - 1);
		return;
	}
	TryPostRoomDelete();
}

void TetrisRoom::ProcessRoomTick(long long tick_time_ms)
{
	ProcessRoomTasks();
	ProcessSessionTasks();
	ProcessRoomTasks();
	const std::size_t task_count = play_tasks_.ClaimTaskCount();
	if (room_state_.load() != RoomState::PLAY) {
		for (std::size_t i = 0; i < task_count; ++i) play_tasks_.Dequeue();
		return;
	}

	const std::uint64_t current_play_generation = play_generation_.load();
	auto room_players = GetRoomPlayers();
	for (std::size_t i = 0; i < task_count; ++i) {
		PlayerInputTask task = play_tasks_.Dequeue();
		if (task.play_generation != current_play_generation) continue;
		for (auto& room_player : room_players) {
			auto session = FindSession(room_player);
			if (!session) continue;
			if (session->GetDBInfo().player_id == task.player_id) {
				// 각 작업들을 각 세션에 분배
				room_player.GetTetris().GetInputTasks().emplace_back(task.event_type);

				break;
			}
		}
	}
	ProcessGameTick(tick_time_ms);
}

void TetrisRoom::CompleteRoomInitialization()
{
	auto room_players = GetRoomPlayers();
	if (!room_players.empty() && room_players[0].HasSession()) room_players[0].Activate(room_players[0].GetSessionKey());
	StoreRoomState(RoomState::WAIT);
	processing_state_.store(RoomProcessState::COMPLETE);
}

void TetrisRoom::BeginRoomDelete()
{
	StoreRoomState(RoomState::WAITING_DELETE);
}

void TetrisRoom::TryPostRoomDelete()
{
	if (room_state_.load() != RoomState::WAITING_DELETE || room_tasks_.GetTaskCount() != 0) return;
	StoreRoomState(RoomState::DELETE_POST);
	server_->EnqueueRoomLifecycleTask(std::make_unique<RoomLifecycleTask>(room_index_));
}

void TetrisRoom::InitGame()
{
	auto room_players = GetRoomPlayers();
	for (auto& room_player : room_players) {
		auto session = FindSession(room_player);
		if (!session) continue;
		room_player.GetTetris().Clear();
	}
}

void TetrisRoom::ClearGame()
{
	StoreRoomState(RoomState::WAIT);
	ClearPlayTasks();
	tetromino_spawn_list_.clear();
	auto room_players = GetRoomPlayers();
	for (auto& room_player : room_players) {
		auto session = FindSession(room_player);
		if (!session) continue;
		room_player.ResetGameData();
	}
}

void TetrisRoom::AppendTetromino7Bag()
{
	std::random_device rd;
	std::mt19937 gen(rd());

	std::vector<int> tetromino_bag = { 0, 1, 2, 3, 4, 5, 6 }; // I, J, L, O, S, T, Z

	std::shuffle(tetromino_bag.begin(), tetromino_bag.end(), gen);
	tetromino_spawn_list_.reserve(tetromino_spawn_list_.size() + tetromino_bag.size());
	tetromino_spawn_list_.insert(tetromino_spawn_list_.end(), tetromino_bag.begin(), tetromino_bag.end());
}

bool TetrisRoom::SpawnTetromino(int player_id) // 내가 이걸 왜 반환형을 bool이라고 했을까
{
	auto room_players = GetRoomPlayers();
	for (auto& room_player : room_players) {
		auto session = FindSession(room_player);
		if (!session) continue;
		if (session->GetDBInfo().player_id == player_id){
			room_player.GetTetris().InitNewTetromino((tetromino_spawn_list_[room_player.GetTetrominoIndex()]), spawn_pos_);
			return true;
		}
	}

	return false;
}

void TetrisRoom::ClearRoom()
{
	ClearPlayTasks();
	tetromino_spawn_list_.clear();
	auto room_players = GetRoomPlayers();
	SessionKey disconnected_session_keys[MAX_MATCH_RESULT_PLAYERS]{};
	int disconnected_session_count = 0;
	for (auto& room_player : room_players) {
		const SessionKey session_key = ClearPlayer(room_player);
		if (session_key.session_index >= 0) disconnected_session_keys[disconnected_session_count++] = session_key;
	}
	current_player_count_.store(0);
	StoreRoomState(RoomState::EMPTY);
	for (int i = 0; i < disconnected_session_count; ++i)
		server_->CompleteRoomDisconnect(disconnected_session_keys[i]);
}

// 얘는 순차적으로 쌓인 작업을 처리해 보내야할 패킷들을 버퍼에 쌓음
SessionKey TetrisRoom::AppendTickPackets()
{
	auto room_players = GetRoomPlayers();
	for (auto& room_player : room_players) {
		if (room_player.GetRoomPlayerState() != RoomPlayerState::PLAY) continue; // 게임오버 되어도 상태 변경은 여기서 이루어지므로 진입 시에는 게임오버 상태는 아님
		auto session = FindSession(room_player);
		if (!session) continue;
		const SessionKey session_key = session->GetSessionKey();
		int player_id = session->GetDBInfo().player_id;

		for (auto& task : room_player.GetTetris().GetSendTasks()) {
			switch (task.event_type) {
			case EventType::MOVE: {
				// 각 이동의 다음 입력 허용 시각은 Tetris::ProcessMoveInput에서 갱신한다.
				auto& move_task = std::get<TaskMove>(task.task);
				if (!AppendMovePacket(room_player, static_cast<int>(move_task.move_type))) return session_key;
				break;
			}

			case EventType::FIX: {
				auto& fix_task = std::get<TaskFix>(task.task);
				S2C_FIX_PACKET fix_p;
				fix_p.header.size = static_cast<std::uint16_t>(sizeof(fix_p));
				fix_p.header.type = S2C_FIX;
				fix_p.player_id = player_id;
				fix_p.fixed_x = fix_task.fixed_x; // 실시간 반영된 값을 읽는게 아니라 작업 목록을 가져와서 패킷을 구성하므로, 작업 당시의 값을 가져와야 함. addline과 동시 틱에 처리되면 클라는 공중에 떠 있는 것으로 보이는 버그 발생
				fix_p.fixed_y = fix_task.fixed_y;
				if (!room_player.AddToSendBuffer(reinterpret_cast<char*>(&fix_p), fix_p.header.size)) return session_key;
				break;
			}

			case EventType::CLEAR_LINE: {
				auto& clear_line_task = std::get<TaskClearLine>(task.task);
				S2C_CLEAR_LINE_PACKET clear_line_p;
				clear_line_p.header.size = static_cast<std::uint16_t>(sizeof(clear_line_p));
				clear_line_p.header.type = S2C_CLEAR_LINE;
				clear_line_p.player_id = player_id;
				clear_line_p.score = room_player.GetScore();
				clear_line_p.line_index = clear_line_task.line_index;
				clear_line_p.combo = room_player.GetCombo();
				if (!room_player.AddToSendBuffer(reinterpret_cast<char*>(&clear_line_p), clear_line_p.header.size)) return session_key;

				break;
			}
			case EventType::SPAWN: {
				if (room_player.GetTetrominoIndex() == (tetromino_spawn_list_.size() - 2)) AppendTetromino7Bag();
				room_player.IncrementTetrominoIndex();

				if (SpawnTetromino(player_id)) {
					S2C_SPAWN_PACKET spawn_p;
					spawn_p.header.size = static_cast<std::uint16_t>(sizeof(spawn_p));
					spawn_p.header.type = S2C_SPAWN;
					spawn_p.player_id = player_id;
					spawn_p.tetromino_type = tetromino_spawn_list_[room_player.GetTetrominoIndex()];
					spawn_p.next_tetromino_type = tetromino_spawn_list_[room_player.GetTetrominoIndex() + 1];
					spawn_p.spawn_x = static_cast<char>(spawn_pos_.x);
					spawn_p.spawn_y = static_cast<char>(spawn_pos_.y);
					if (!room_player.AddToSendBuffer(reinterpret_cast<char*>(&spawn_p), spawn_p.header.size)) return session_key;
				}
				break;
			}

			case EventType::ADD_LINE: {
				S2C_ADD_LINE_PACKET add_line_p;
				add_line_p.header.size = static_cast<std::uint16_t>(sizeof(add_line_p));
				add_line_p.header.type = S2C_ADD_LINE;
				add_line_p.player_id = player_id;
				auto& add_line_task = std::get<TaskAddLine>(task.task);
				add_line_p.hole_x = static_cast<char>(add_line_task.hole_x);
				if (!room_player.AddToSendBuffer(reinterpret_cast<char*>(&add_line_p), add_line_p.header.size)) return session_key;
				break;
			}

			case EventType::GAME_OVER: {
				// 일단 종료 패킷을 보냄
				room_player.SetRoomPlayerState(RoomPlayerState::GAME_OVER);
				S2C_GAME_OVER_PACKET game_over_p;
				game_over_p.header.size = static_cast<std::uint16_t>(sizeof(game_over_p));
				game_over_p.header.type = S2C_GAME_OVER;
				game_over_p.player_id = player_id;
				if (!room_player.AddToSendBuffer(reinterpret_cast<char*>(&game_over_p), game_over_p.header.size)) return session_key;

				break;
			}

			case EventType::GAME_END: { // 이건 사실상 멀티만 쓰므로.. 근데 이거 하나때문에 또 분리하기 좀 그렇긴 하다 분리하는게 좋긴 할 것 같지만..
				auto& game_end_task = std::get<TaskGameEnd>(task.task);

				S2C_GAME_END_PACKET game_end_p;
				game_end_p.header.size = static_cast<std::uint16_t>(sizeof(game_end_p));
				game_end_p.header.type = S2C_GAME_END;
				game_end_p.winner_id = game_end_task.winner_id;
				if (!room_player.AddToSendBuffer(reinterpret_cast<char*>(&game_end_p), game_end_p.header.size)) return session_key;
				break;
			}
			}
		}
	}
	return {};
}

bool TetrisRoom::AppendMovePacket(Player& player, int move_type)
{
	auto session = FindSession(player);
	if (!session) return true;
	S2C_MOVE_PACKET move_p;
	move_p.header.size = static_cast<std::uint16_t>(sizeof(move_p));
	move_p.header.type = S2C_MOVE;
	move_p.player_id = session->GetDBInfo().player_id;
	move_p.move_type = static_cast<char>(move_type);
	return player.AddToSendBuffer(reinterpret_cast<char*>(&move_p), move_p.header.size);
}

void TetrisRoom::AddGarbageLines()
{
	auto room_players = GetRoomPlayers();
	for (auto& room_player : room_players) {
		if (room_player.IsActive() && room_player.GetRoomPlayerState() == RoomPlayerState::PLAY) {
			room_player.GetTetris().AddGarbageLines();
		}
	}
}

void TetrisRoom::AddSpawnTasks()
{
	// spawn은 룸에서 이루어져야 한다. 스폰될 테트로미노를 일괄 관리중이기 때문이다.
	// spawn은 fix와 항상 같이 일어나므로, 작업에서 fix 여부를 확인해 있으면 추가해준다.
	auto room_players = GetRoomPlayers();
	for (auto& room_player : room_players) {
		if (!room_player.IsActive() || room_player.GetRoomPlayerState() != RoomPlayerState::PLAY) continue;
		std::vector<TaskType>& tasks = room_player.GetTetris().GetSendTasks();
		bool does_fix_exist = false;
		for (auto& task : tasks) {
			if (task.event_type == EventType::FIX) {
				does_fix_exist = true;
				break;
			}
		}

		if (does_fix_exist) {
			TaskType task;
			task.event_type = EventType::SPAWN;
			room_player.GetTetris().GetSendTasks().emplace_back(task);
		}
	}
}

void TetrisRoom::ResetPlayerTickState()
{
	auto room_players = GetRoomPlayers();
	for(auto& room_player : room_players){
		auto session = FindSession(room_player);
		if (!session) continue;
		room_player.GetTetris().ResetTickData();
	}
}

void TetrisRoom::BroadcastPackets()
{
	auto room_players = GetRoomPlayers();
	char send_buffer[BUF_SIZE];
	int send_data_size = 0;

	for (int i = 0; i < room_players.size(); ++i) {
		Player& source = room_players[i];
		auto source_session = FindSession(source);
		if (!source_session) continue;
		const int source_data_size = source.GetSendDataSize();
		if (source_data_size <= 0) continue;

		if (send_data_size + source_data_size > BUF_SIZE) {
			for (auto& target : room_players) {
				auto target_session = FindSession(target);
				if (!target_session) continue;
				target_session->SendPacket(send_buffer, send_data_size, server_->GetIOCPHandle());
			}
			send_data_size = 0;
		}

		memcpy(send_buffer + send_data_size, source.GetSendBuffer(), source_data_size);
		send_data_size += source_data_size;
	}

	if (send_data_size > 0) {
		for (auto& target : room_players) {
			auto target_session = FindSession(target);
			if (!target_session) continue;
			target_session->SendPacket(send_buffer, send_data_size, server_->GetIOCPHandle());
		}
	}

	for (auto& room_player : room_players) {
		auto session = FindSession(room_player);
		if (!session) continue;
		room_player.ClearSendBuffer();
	}
}

void TetrisRoom::Broadcast(char* packet, const HANDLE iocp_handle)
{
	// 범위기반을 있다고 생각하고 만들었는데, 그 순회하는 사이에 접근하기 전에 삭제되면 세션 포인터 nullptr 오류가 생긴다.
	// 결국 락을 걸 수밖에..
	// 틱 루프에서 호출할 경우 이중락 걸리므로 주의
	// 우선 외부에서 잠그는걸로 다시 변경. 범위기반 탐색과 삭제 사이의 관계 때문에 락이 필요한데, 멀티와 같은 경우 범위기반 탐색 내에 다시 범위기반 탐색을 하는 경우도 꽤 있으므로 외부에서 하는게 효율적인 것 같다.
	std::vector<int> target_index;
	const int packet_size = static_cast<int>(reinterpret_cast<const PACKET_HEADER*>(packet)->size);
	auto room_players = GetRoomPlayers();
	for (auto& room_player : room_players) {
		auto session = FindSession(room_player);
		if (!session) continue;
		session->SendPacket(packet, packet_size, iocp_handle);
	}
}

void TetrisRoom::StoreRoomState(RoomState new_state)
{
	room_state_.store(new_state);
}

bool TetrisRoom::TryChangeRoomState(RoomState expected, RoomState desired)
{
	return room_state_.compare_exchange_strong(expected, desired);
}
