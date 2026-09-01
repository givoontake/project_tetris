#include <random>
#include <utility>
#include "TetrisRoom.h"
#include "IOCPServer.h"
#include "packet_types.h"

TetrisRoom::TetrisRoom(IOCPServer* server, OpenRoomInitData data)
{
	// 생성과 소멸은 스레드 세이프하지는 않지만, 어차피 make_shared하고 CAS해서 룸 리스트에 할당하기 전에는 접근되지 않는다.
	server_ = server;
	max_user_ = data.max_user;
	room_name_ = std::move(data.room_name);
	room_password_.clear();
	room_index_ = data.room_index;
	room_gen_ = data.room_gen;
	room_state_.store(RoomState::EMPTY);

	//SendAddRoom(session);
	cur_user_.store(0);
}

TetrisRoom::TetrisRoom(IOCPServer* server, LockRoomInitData data)
{
	server_ = server;
	max_user_ = data.max_user;
	room_name_ = std::move(data.room_name);
	room_password_ = std::move(data.room_password);
	room_index_ = data.room_index;
	room_gen_ = data.room_gen;
	room_state_.store(RoomState::EMPTY);
	//SendAddRoom(session);
	cur_user_.store(0);
}

TetrisRoom::~TetrisRoom()
{
}

int TetrisRoom::GetCurrentUser() const
{
	return static_cast<int>(cur_user_.load());
}

bool TetrisRoom::InitHostSession(const SP<Session>& session)
{
	if (!session) return false;
	auto room_users = GetRoomUsers();
	if (room_users.empty()) return false;
	if (room_users[0].GetSession()) return false;
	if (!room_users[0].InitRoomSession(session, room_index_)) return false;
	cur_user_.store(1);
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
	snapshot.max_user = static_cast<int>(max_user_);
	snapshot.cur_user = static_cast<int>(cur_user_.load());
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

bool TetrisRoom::IsRoomSession(const SP<Session>& session) const
{
	if (!session) return false;
	const RoomSnapShot snapshot = session->GetRoomSnapShot();
	return snapshot.state == ModeState::ROOM && snapshot.room_index == room_index_;
}

void TetrisRoom::HandlePacket(char* packet, const SP<Session>& request_session)
{
	if (!request_session) return;
	switch (reinterpret_cast<PACKET_HEADER*>(packet)->type) {
	case C2S_DELETE_USER: {
		RoomTaskInfo task;
		task.type = RoomTaskType::DELETE_USER;
		task.session = request_session;
		AddRoomTask(std::move(task));
		break;
	}
	case C2S_START: {
		RoomTaskInfo task;
		task.type = RoomTaskType::START;
		task.session = request_session;
		AddRoomTask(std::move(task));
		break;
	}
	case C2S_MOVE: {
		if (!IsRoomSession(request_session)) return;
		const std::uint64_t current_play_generation = play_generation_.load();
		if (room_state_.load() != RoomState::PLAY) return;
		C2S_MOVE_PACKET* recv_p = reinterpret_cast<C2S_MOVE_PACKET*>(packet);
		TaskInfo task;
		task.id = request_session->GetDBInfo().id;
		task.type = static_cast<EventType>(recv_p->move_type);
		task.play_generation = current_play_generation;
		AddPlayTask(std::move(task));
		break;
	}
	default:
		break;
	}
}

bool TetrisRoom::AddRoomTask(RoomTaskInfo task)
{
	RoomState state = room_state_.load();
	if (state != RoomState::WAIT && state != RoomState::PLAY) return false;
	room_tasks_.Enqueue(std::move(task));
	state = room_state_.load();
	return state == RoomState::WAIT || state == RoomState::PLAY;
}

void TetrisRoom::AddPlayTask(TaskInfo task)
{
	play_tasks_.Enqueue(std::move(task));
}

void TetrisRoom::ProcessRoomTasks()
{
	const std::size_t task_count = room_tasks_.ClaimTaskCount();
	for (std::size_t i = 0; i < task_count; ++i) {
		RoomTaskInfo task = room_tasks_.Dequeue();
		if (!task.session) continue;

		if (task.type == RoomTaskType::DELETE_USER) {
			if (IsRoomSession(task.session)) DeleteUser(task.session->GetDBInfo().id);
			continue;
		}
		ProcessSpecificRoomTask(task);
	}
	TryPostRoomDelete();
}

void TetrisRoom::ProcessPlayTasks()
{
	ProcessRoomTasks();
	const std::size_t task_count = play_tasks_.ClaimTaskCount();
	if (room_state_.load() != RoomState::PLAY) {
		for (std::size_t i = 0; i < task_count; ++i) play_tasks_.Dequeue();
		return;
	}

	UpdateTick();
	const std::uint64_t current_play_generation = play_generation_.load();
	auto room_users = GetRoomUsers();
	for (std::size_t i = 0; i < task_count; ++i) {
		TaskInfo task = play_tasks_.Dequeue();
		if (task.play_generation != current_play_generation) continue;
		for (auto& r_user : room_users) {
			auto session = r_user.GetSession();
			if (!session) continue;
			if (session->GetDBInfo().id == task.id) {
				// 각 작업들을 각 세션에 분배
				r_user.GetTetris().GetInputTasks().emplace_back(task.type);

				//r_user.GetTetris().DebugPrintBoard();
				break;
			}
		}
	}
	ProcessGameTick();
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
	PostQueuedCompletionStatus(server_->GetHandle(), 1, ROOM_IO_COMPLETION, reinterpret_cast<WSAOVERLAPPED*>(delete_over));
}

void TetrisRoom::InitGame()
{
	auto room_users = GetRoomUsers();
	for (auto& r_user : room_users) { 
		auto session = r_user.GetSession();
		if (!session) continue;
		r_user.GetTetris().Clear();
	}
}

void TetrisRoom::ClearGame()
{
	StoreRoomState(RoomState::WAIT);
	ClearPlayTasks();
	tetromino_spawn_list_.clear();
	auto room_users = GetRoomUsers();
	for (auto& r_user : room_users) {
		auto session = r_user.GetSession();
		if (!session) continue;
		r_user.ClearData();
	}
}

void TetrisRoom::Add7BagTetrominoList()
{
	std::random_device rd;
	std::mt19937 gen(rd());

	std::vector<int> v = { 0, 1, 2, 3, 4, 5, 6 }; // I, J, L, O, S, T, Z

	std::shuffle(v.begin(), v.end(), gen);
	tetromino_spawn_list_.reserve(tetromino_spawn_list_.size() + v.size());
	tetromino_spawn_list_.insert(tetromino_spawn_list_.end(), v.begin(), v.end());
}

bool TetrisRoom::SpawnTetromino(int id) // 내가 이걸 왜 반환형을 bool이라고 했을까
{
	auto room_users = GetRoomUsers();
	for (auto& r_user : room_users) {
		auto session = r_user.GetSession();
		if (!session) continue;
		if (session->GetDBInfo().id == id){
			r_user.GetTetris().InitNewTetromino((tetromino_spawn_list_[r_user.GetTetrominoIndex()]), spawn_pos_);
			return true;
		}
	}

	return false;
}

void TetrisRoom::ClearRoom()
{
	ClearPlayTasks();
	tetromino_spawn_list_.clear();
	auto room_users = GetRoomUsers();
	for (auto& r_user : room_users) r_user.ClearRoomSession();
	cur_user_.store(0);
	StoreRoomState(RoomState::EMPTY);
}

void TetrisRoom::UpdateTick()
{
	auto room_users = GetRoomUsers();
	for(auto& r_user : room_users){
		auto session = r_user.GetSession();
		if (!session) continue;
		r_user.GetTetris().GetTickData().UpdateTickData();
	}
}

// 얘는 순차적으로 쌓인 작업을 처리해 보내야할 패킷들을 버퍼에 쌓음
int TetrisRoom::BoundPackets()
{
	auto room_users = GetRoomUsers();
	for (auto& r_user : room_users) {
		if (r_user.GetRoomUserState() != RoomUserState::PLAY) continue; // 게임오버 되어도 상태 변경은 여기서 이루어지므로 진입 시에는 게임오버 상태는 아님
		auto session = r_user.GetSession();
		if (!session) continue;
		int user_id = session->GetDBInfo().id;

		for (auto& task : r_user.GetTetris().GetSendTasks()) {
			switch (task.event_type) {
			case EventType::MOVE: {
				// 각 무브별 틱 초기화 추가가 애매하므로, 무브 틱 값 초기화는 Tetris::HandleTetrominoKeyInput에서 처리
				auto& t = std::get<TaskMove>(task.task);
				if (!MakeMovePacket(r_user, static_cast<int>(t.move_type))) return user_id;
				break;
			}

			case EventType::FIX: {
				auto& t = std::get<TaskFix>(task.task);
				S2C_FIX_PACKET fix_p;
				fix_p.header.size = static_cast<std::uint16_t>(sizeof(fix_p));
				fix_p.header.type = S2C_FIX;
				fix_p.id = user_id;
				fix_p.fixed_x = t.fixed_x; // 실시간 반영된 값을 읽는게 아니라 작업 목록을 가져와서 패킷을 구성하므로, 작업 당시의 값을 가져와야 함. addline과 동시 틱에 처리되면 클라는 공중에 떠 있는 것으로 보이는 버그 발생
				fix_p.fixed_y = t.fixed_y;
				if (!r_user.AddToSendBuffer(reinterpret_cast<char*>(&fix_p), fix_p.header.size)) return user_id;
				break;
			}

			case EventType::CLEARLINE: {
				auto& t = std::get<TaskClearLine>(task.task);
				S2C_CLEARLINE_PACKET clear_line_p;
				clear_line_p.header.size = static_cast<std::uint16_t>(sizeof(clear_line_p));
				clear_line_p.header.type = S2C_CLEARLINE;
				clear_line_p.id = user_id;
				clear_line_p.score = r_user.GetScore();
				clear_line_p.line_index = t.line_index;
				clear_line_p.combo = r_user.GetCombo();
				if (!r_user.AddToSendBuffer(reinterpret_cast<char*>(&clear_line_p), clear_line_p.header.size)) return user_id;

				break;
			}
			case EventType::SPAWN: {
				if (r_user.GetTetrominoIndex() == (tetromino_spawn_list_.size() - 2)) Add7BagTetrominoList();
				r_user.AddTetrominoIndex();

				if (SpawnTetromino(user_id)) {
					S2C_SPAWN_PACKET spawn_p;
					spawn_p.header.size = static_cast<std::uint16_t>(sizeof(spawn_p));
					spawn_p.header.type = S2C_SPAWN;
					spawn_p.id = user_id;
					spawn_p.tetromino_type = tetromino_spawn_list_[r_user.GetTetrominoIndex()];
					spawn_p.next_tetromino_type = tetromino_spawn_list_[r_user.GetTetrominoIndex() + 1];
					spawn_p.spawn_x = static_cast<char>(spawn_pos_.x);
					spawn_p.spawn_y = static_cast<char>(spawn_pos_.y);
					if (!r_user.AddToSendBuffer(reinterpret_cast<char*>(&spawn_p), spawn_p.header.size)) return user_id;
				}
				break;
			}

			case EventType::ADDLINE: {
				r_user.GetTetris().GetTickData().SetGarbageLineTick(0);
				S2C_ADDLINE_PACKET add_line_p;
				add_line_p.header.size = static_cast<std::uint16_t>(sizeof(add_line_p));
				add_line_p.header.type = S2C_ADDLINE;
				add_line_p.id = user_id;
				auto& t = std::get<TaskAddLine>(task.task);
				add_line_p.hole_x = static_cast<char>(t.hole_x);
				if (!r_user.AddToSendBuffer(reinterpret_cast<char*>(&add_line_p), add_line_p.header.size)) return user_id;
				break;
			}

			case EventType::GAMEOVER: {
				// 일단 종료 패킷을 보냄
				r_user.SetRoomUserState(RoomUserState::GAMEOVER);
				S2C_GAMEOVER_PACKET gameover_p;
				gameover_p.header.size = static_cast<std::uint16_t>(sizeof(gameover_p));
				gameover_p.header.type = S2C_GAMEOVER;
				gameover_p.id = user_id;
				if (!r_user.AddToSendBuffer(reinterpret_cast<char*>(&gameover_p), gameover_p.header.size)) return user_id;

				break;
			}

			case EventType::GAMEEND: { // 이건 사실상 멀티만 쓰므로.. 근데 이거 하나때문에 또 분리하기 좀 그렇긴 하다 분리하는게 좋긴 할 것 같지만..
				auto& t = std::get<TaskGameEnd>(task.task);

				S2C_GAMEEND_PACKET gameend_p;
				gameend_p.header.size = static_cast<std::uint16_t>(sizeof(gameend_p));
				gameend_p.header.type = S2C_GAMEEND;
				gameend_p.winner_id = t.winner_id;
				if (!r_user.AddToSendBuffer(reinterpret_cast<char*>(&gameend_p), gameend_p.header.size)) return user_id;
				break;
			}
			}
		}
	}
	return -1;
}

bool TetrisRoom::MakeMovePacket(RoomSession& r_session, int move_type)
{
	auto session = r_session.GetSession();
	if (!session) return true;
	S2C_MOVE_PACKET move_p;
	move_p.header.size = static_cast<std::uint16_t>(sizeof(move_p));
	move_p.header.type = S2C_MOVE;
	move_p.id = session->GetDBInfo().id;
	move_p.move_type = static_cast<char>(move_type);
	return r_session.AddToSendBuffer(reinterpret_cast<char*>(&move_p), move_p.header.size);
}

void TetrisRoom::AddGarbageLines()
{
	auto room_users = GetRoomUsers();
	for (auto& r_user : room_users) {
		if (r_user.GetRoomUserState() == RoomUserState::PLAY) {
			r_user.GetTetris().AddGarbageLines();
		}
	}
}

void TetrisRoom::AddSpawnTask()
{
	// spawn은 룸에서 이루어져야 한다. 스폰될 테트로미노를 일괄 관리중이기 때문이다.
	// spawn은 fix와 항상 같이 일어나므로, 작업에서 fix 여부를 확인해 있으면 추가해준다.
	auto room_users = GetRoomUsers();
	for (auto& r_user : room_users) {
		if (r_user.GetRoomUserState() != RoomUserState::PLAY) continue;
		std::vector<TaskType>& tasks = r_user.GetTetris().GetSendTasks();
		bool does_fix_exist = false;
		for (auto& task : tasks) {
			if (task.event_type == EventType::FIX) {
				does_fix_exist = true;
				break;
			}
		}

		if (does_fix_exist) {
			TaskType t_type;
			t_type.event_type = EventType::SPAWN;
			r_user.GetTetris().GetSendTasks().emplace_back(t_type);
		}
	}
}

void TetrisRoom::ResetUsersTickData()
{
	auto room_users = GetRoomUsers();
	for(auto& r_user : room_users){
		auto session = r_user.GetSession();
		if (!session) continue;
		r_user.GetTetris().ResetTickData();
	}
}

void TetrisRoom::BroadcastTickDataForUsers()
{
	auto room_users = GetRoomUsers();
	for (int i = 0; i < room_users.size(); ++i) {
		RoomSession& source = room_users[i];
		auto source_session = source.GetSession();
		if (!source_session) continue;
		//IOKey key = source.GetSession()->GetIOKey();
		char* send_buffer = source.GetSendBuf();
		int data_size = source.GetSendDataSize();

		for (int j = 0; j < room_users.size(); ++j) {
			RoomSession& target = room_users[j];
			// data_size = 0이 IOCP로 들어가면 연결이 종료되는 것에 주의해야함
			// 게임 시작 시 틱마다 자동 전송하므로 조건이 꼭 필요
			if (data_size > 0) {
				auto target_session = target.GetSession();
				if (!target_session) continue;
				target_session->SendBoundPacket(send_buffer, data_size, server_->GetHandle());
			}
		}
	}

	for (auto& r_user : room_users) {
		auto session = r_user.GetSession();
		if (!session) continue;
		r_user.ClearSendBuf();
	}
}

void TetrisRoom::Broadcast(char* packet, const HANDLE iocp_handle)
{
	// 범위기반을 있다고 생각하고 만들었는데, 그 순회하는 사이에 접근하기 전에 삭제되면 세션 포인터 nullptr 오류가 생긴다.
	// 결국 락을 걸 수밖에..
	// 틱 루프에서 호출할 경우 이중락 걸리므로 주의
	// 우선 외부에서 잠그는걸로 다시 변경. 범위기반 탐색과 삭제 사이의 관계 때문에 락이 필요한데, 멀티와 같은 경우 범위기반 탐색 내에 다시 범위기반 탐색을 하는 경우도 꽤 있으므로 외부에서 하는게 효율적인 것 같다.
	std::vector<int> target_index;
	auto room_users = GetRoomUsers();
	for (auto& r_user : room_users) {
		auto session = r_user.GetSession();
		if (!session) continue;
		session->SendPacket(packet, iocp_handle);
	}
}

void TetrisRoom::SetRoomIndex(const int val)
{
	room_index_ = val;
}

void TetrisRoom::SetRoomGen(const int val)
{
	room_gen_ = val;
}

void TetrisRoom::StoreRoomState(RoomState new_state)
{
	room_state_.store(new_state);
}

bool TetrisRoom::TryChangeRoomState(RoomState expected, RoomState desired)
{
	return room_state_.compare_exchange_strong(expected, desired);
}
