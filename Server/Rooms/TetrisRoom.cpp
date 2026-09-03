#include <random>
#include <utility>
#include "TetrisRoom.h"
#include "IOCPServer.h"
#include "packet_types.h"

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

bool TetrisRoom::InitHostSession(const SP<Session>& session)
{
	if (!session) return false;
	auto room_players = GetRoomPlayers();
	if (room_players.empty()) return false;
	if (room_players[0].GetSession()) return false;
	if (!room_players[0].InitPlayer(session, room_index_)) return false;
	current_player_count_.store(1);
	return true;
}

bool TetrisRoom::AddHostSession(const SP<Session>& session)
{
	return InitHostSession(session);
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

bool TetrisRoom::IsPlayerInRoom(const SP<Session>& session) const
{
	if (!session) return false;
	const RoomSnapshot snapshot = session->GetRoomSnapshot();
	return snapshot.mode_state == ModeState::ROOM && snapshot.room_index == room_index_;
}

void TetrisRoom::HandlePacket(char* packet, const SP<Session>& request_session)
{
	if (!request_session) return;
	switch (reinterpret_cast<PACKET_HEADER*>(packet)->type) {
	case C2S_REMOVE_PLAYER: {
		RoomTask task;
		task.task_type = RoomTaskType::REMOVE_PLAYER;
		task.session = request_session;
		AddRoomTask(std::move(task));
		break;
	}
	case C2S_START: {
		RoomTask task;
		task.task_type = RoomTaskType::START;
		task.session = request_session;
		AddRoomTask(std::move(task));
		break;
	}
	case C2S_MOVE: {
		if (!IsPlayerInRoom(request_session)) return;
		const std::uint64_t current_play_generation = play_generation_.load();
		if (room_state_.load() != RoomState::PLAY) return;
		C2S_MOVE_PACKET* recv_p = reinterpret_cast<C2S_MOVE_PACKET*>(packet);
		PlayerInputTask task;
		task.player_id = request_session->GetDBInfo().player_id;
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
	return state == RoomState::WAIT || state == RoomState::PLAY;
}

void TetrisRoom::AddPlayTask(PlayerInputTask task)
{
	play_tasks_.Enqueue(std::move(task));
}

void TetrisRoom::ProcessRoomTasks()
{
	const std::size_t task_count = room_tasks_.ClaimTaskCount();
	for (std::size_t i = 0; i < task_count; ++i) {
		RoomTask task = room_tasks_.Dequeue();
		if (!task.session) continue;

		if (task.task_type == RoomTaskType::REMOVE_PLAYER) {
			if (IsPlayerInRoom(task.session)) RemovePlayer(task.session->GetDBInfo().player_id);
			continue;
		}
		ProcessSpecificRoomTask(task);
	}
	TryPostRoomDelete();
}

void TetrisRoom::ProcessRoomTick(std::chrono::steady_clock::time_point tick_time)
{
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
			auto session = room_player.GetSession();
			if (!session) continue;
			if (session->GetDBInfo().player_id == task.player_id) {
				// 각 작업들을 각 세션에 분배
				room_player.GetTetris().GetInputTasks().emplace_back(task.event_type);

				break;
			}
		}
	}
	ProcessGameTick(tick_time);
}

void TetrisRoom::CompleteRoomInitialization()
{
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
	ExOverlapped* delete_over = new ExOverlapped;
	delete_over->op_type = OPType::DELETE_ROOM;
	delete_over->room_index = room_index_;
	PostQueuedCompletionStatus(server_->GetIOCPHandle(), 1, ROOM_IO_COMPLETION, reinterpret_cast<WSAOVERLAPPED*>(delete_over));
}

void TetrisRoom::InitGame()
{
	auto room_players = GetRoomPlayers();
	for (auto& room_player : room_players) {
		auto session = room_player.GetSession();
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
		auto session = room_player.GetSession();
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
		auto session = room_player.GetSession();
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
	for (auto& room_player : room_players) room_player.ClearPlayer();
	current_player_count_.store(0);
	StoreRoomState(RoomState::EMPTY);
}

// 얘는 순차적으로 쌓인 작업을 처리해 보내야할 패킷들을 버퍼에 쌓음
int TetrisRoom::AppendTickPackets()
{
	auto room_players = GetRoomPlayers();
	for (auto& room_player : room_players) {
		if (room_player.GetRoomPlayerState() != RoomPlayerState::PLAY) continue; // 게임오버 되어도 상태 변경은 여기서 이루어지므로 진입 시에는 게임오버 상태는 아님
		auto session = room_player.GetSession();
		if (!session) continue;
		int player_id = session->GetDBInfo().player_id;

		for (auto& task : room_player.GetTetris().GetSendTasks()) {
			switch (task.event_type) {
			case EventType::MOVE: {
				// 각 이동의 다음 입력 허용 시각은 Tetris::ProcessMoveInput에서 갱신한다.
				auto& move_task = std::get<TaskMove>(task.task);
				if (!AppendMovePacket(room_player, static_cast<int>(move_task.move_type))) return player_id;
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
				if (!room_player.AddToSendBuffer(reinterpret_cast<char*>(&fix_p), fix_p.header.size)) return player_id;
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
				if (!room_player.AddToSendBuffer(reinterpret_cast<char*>(&clear_line_p), clear_line_p.header.size)) return player_id;

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
					if (!room_player.AddToSendBuffer(reinterpret_cast<char*>(&spawn_p), spawn_p.header.size)) return player_id;
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
				if (!room_player.AddToSendBuffer(reinterpret_cast<char*>(&add_line_p), add_line_p.header.size)) return player_id;
				break;
			}

			case EventType::GAME_OVER: {
				// 일단 종료 패킷을 보냄
				room_player.SetRoomPlayerState(RoomPlayerState::GAME_OVER);
				S2C_GAME_OVER_PACKET game_over_p;
				game_over_p.header.size = static_cast<std::uint16_t>(sizeof(game_over_p));
				game_over_p.header.type = S2C_GAME_OVER;
				game_over_p.player_id = player_id;
				if (!room_player.AddToSendBuffer(reinterpret_cast<char*>(&game_over_p), game_over_p.header.size)) return player_id;

				break;
			}

			case EventType::GAME_END: { // 이건 사실상 멀티만 쓰므로.. 근데 이거 하나때문에 또 분리하기 좀 그렇긴 하다 분리하는게 좋긴 할 것 같지만..
				auto& game_end_task = std::get<TaskGameEnd>(task.task);

				S2C_GAME_END_PACKET game_end_p;
				game_end_p.header.size = static_cast<std::uint16_t>(sizeof(game_end_p));
				game_end_p.header.type = S2C_GAME_END;
				game_end_p.winner_id = game_end_task.winner_id;
				if (!room_player.AddToSendBuffer(reinterpret_cast<char*>(&game_end_p), game_end_p.header.size)) return player_id;
				break;
			}
			}
		}
	}
	return -1;
}

bool TetrisRoom::AppendMovePacket(Player& player, int move_type)
{
	auto session = player.GetSession();
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
		if (room_player.GetRoomPlayerState() == RoomPlayerState::PLAY) {
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
		if (room_player.GetRoomPlayerState() != RoomPlayerState::PLAY) continue;
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
		auto session = room_player.GetSession();
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
		auto source_session = source.GetSession();
		if (!source_session) continue;
		const int source_data_size = source.GetSendDataSize();
		if (source_data_size <= 0) continue;

		if (send_data_size + source_data_size > BUF_SIZE) {
			for (auto& target : room_players) {
				auto target_session = target.GetSession();
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
			auto target_session = target.GetSession();
			if (!target_session) continue;
			target_session->SendPacket(send_buffer, send_data_size, server_->GetIOCPHandle());
		}
	}

	for (auto& room_player : room_players) {
		auto session = room_player.GetSession();
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
		auto session = room_player.GetSession();
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
