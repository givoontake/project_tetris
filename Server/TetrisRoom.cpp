#include <random>
#include <utility>
#include "TetrisRoom.h"
#include "IOCPServer.h"
#include "packet_types.h"

TetrisRoom::TetrisRoom(IOCPServer* _server, OpenRoomInitData data)
{
	// 생성과 소멸은 스레드 세이프하지는 않지만, 어차피 make_shared하고 CAS해서 룸 리스트에 할당하기 전에는 접근되지 않는다.
	server = _server;
	max_user = data.max_user;
	room_name = std::move(data.room_name);
	room_password.clear();
	room_index = data.room_index;
	room_gen = data.room_gen;
	room_state.Store(ROOM_STATE::WAIT);
	room_users.reserve(max_user); // 미리 메모리를 할당하고 객체를 채우면 문제 x
	for (int i = 0; i < max_user; ++i) {
		room_users.emplace_back(max_user);
	}

	//SendAddRoom(session);
	cur_user = 0;
}

TetrisRoom::TetrisRoom(IOCPServer* _server, LockRoomInitData data)
{
	server = _server;
	max_user = data.max_user;
	room_name = std::move(data.room_name);
	room_password = std::move(data.room_password);
	room_index = data.room_index;
	room_gen = data.room_gen;
	room_state.Store(ROOM_STATE::WAIT);
	room_users.reserve(max_user); // 미리 메모리를 할당하고 객체를 채우면 문제 x
	for (int i = 0; i < max_user; ++i) {
		room_users.emplace_back(max_user);
	}
	//SendAddRoom(session);
	cur_user = 0;
}

TetrisRoom::~TetrisRoom()
{
}

int TetrisRoom::GetCurrentUser() const
{
	int count = 0;
	for (auto& r_user : room_users) {
		if (r_user.GetSession()) ++count;
	}
	return count;
}

bool TetrisRoom::InitHostSession(const SP<Session>& session)
{
	if (!session) return false;
	if (room_users.empty()) return false;
	if (room_users[0].GetSession()) return false;
	if (!room_users[0].InitRoomSession(session, room_index)) return false;
	cur_user = 1;
	return true;
}

bool TetrisRoom::AddHostSession(const SP<Session>& session)
{
	std::lock_guard<std::mutex> lock(room_mutex);
	return InitHostSession(session);
}

int TetrisRoom::AddStressUserInLock(const SP<Session>& session)
{
	if (!session) return ERROR_CODE::INVALID_REQUEST;
	if (room_state == ROOM_STATE::PLAY) return ERROR_CODE::ROOM_INGAME;
	if (room_state == ROOM_STATE::WAITING_DELETE) return ERROR_CODE::ROOM_NOT_FOUND;
	if (room_state != ROOM_STATE::WAIT) return ERROR_CODE::INVALID_REQUEST;

	for (auto& r_user : room_users) {
		if (r_user.GetSession()) continue;
		if (!r_user.InitRoomSession(session, room_index)) return ERROR_CODE::INVALID_REQUEST;
		++cur_user;
		return SUCCESS;
	}

	return ERROR_CODE::ROOM_FULL;
}

int TetrisRoom::AddStressUser(const SP<Session>& session)
{
	std::lock_guard<std::mutex> lock(room_mutex);
	return AddStressUserInLock(session);
}

RoomInfoSnapshot TetrisRoom::GetRoomInfoSnapshot()
{
	std::lock_guard<std::mutex> lock(room_mutex);
	RoomInfoSnapshot snapshot;
	snapshot.room_state = room_state.Load();
	snapshot.room_gen = room_gen;
	snapshot.max_user = static_cast<int>(max_user);
	for (auto& r_user : room_users) {
		if (r_user.GetSession()) ++snapshot.cur_user;
	}
	snapshot.room_name = room_name;
	snapshot.is_private = !room_password.empty();
	return snapshot;
}

void TetrisRoom::InitGame()
{
	for (auto& r_user : room_users) { 
		auto session = r_user.GetSession();
		if (!session) continue;
		r_user.GetTetris().Clear();
	}
}

void TetrisRoom::ClearGame()
{
	room_state = ROOM_STATE::WAIT;
	tasks.Clear();
	tetromino_spawn_list.clear();
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
	tetromino_spawn_list.reserve(tetromino_spawn_list.size() + v.size());
	tetromino_spawn_list.insert(tetromino_spawn_list.end(), v.begin(), v.end());
}

bool TetrisRoom::SpawnTetromino(int id) // 내가 이걸 왜 반환형을 bool이라고 했을까
{
	for (auto& r_user : room_users) {
		auto session = r_user.GetSession();
		if (!session) continue;
		if (session->GetDBInfo().id == id){
			r_user.GetTetris().InitNewTetromino((tetromino_spawn_list[r_user.GetTetrominoIndex()]), spawn_pos);
			return true;
		}
	}

	return false;
}

void TetrisRoom::ClearRoom()
{
	room_state.Store(ROOM_STATE::EMPTY);
	tasks.Clear();
	tetromino_spawn_list.clear();
	room_users.clear();
}

void TetrisRoom::UpdateTick()
{
	for(auto& r_user : room_users){
		auto session = r_user.GetSession();
		if (!session) continue;
		r_user.GetTetris().GetTickData().UpdateTickData();
	}
}

// 얘는 순차적으로 쌓인 작업을 처리해 보내야할 패킷들을 버퍼에 쌓음
int TetrisRoom::BoundPackets()
{
	for (auto& r_user : room_users) {
		if (r_user.GetRoomUserState() != ROOM_USER_STATE::PLAY) continue; // 게임오버 되어도 상태 변경은 여기서 이루어지므로 진입 시에는 게임오버 상태는 아님
		auto session = r_user.GetSession();
		if (!session) continue;
		int user_id = session->GetDBInfo().id;

		for (auto& task : r_user.GetTetris().GetSendTasks()) {
			switch (task.event_type) {
			case EVENT_TYPE::MOVE: {
				// 각 무브별 틱 초기화 추가가 애매하므로, 무브 틱 값 초기화는 Tetris::HandleTetrominoKeyInput에서 처리
				auto& t = std::get<TaskMove>(task.task);
				if (!MakeMovePacket(r_user, static_cast<int>(t.move_type))) return user_id;
				break;
			}

			case EVENT_TYPE::FIX: {
				auto& t = std::get<TaskFix>(task.task);
				S2C_FIX_PACKET fix_p;
				fix_p.header.size = static_cast<std::uint16_t>(sizeof(fix_p));
				fix_p.header.type = S2C_FIX;
				fix_p.id = user_id;
				fix_p.fixed_x = t.fixed_x; // 실시간 반영된 값을 읽는게 아니라 작업 목록을 가져와서 패킷을 구성하므로, 작업 당시의 값을 가져와야 함. addline과 동시 틱에 처리되면 클라는 공중에 떠 있는 것으로 보이는 버그 발생
				fix_p.fixed_y = t.fixed_y;
				if (!r_user.AddToTickBuffer(reinterpret_cast<char*>(&fix_p), fix_p.header.size)) return user_id;
				break;
			}

			case EVENT_TYPE::CLEARLINE: {
				auto& t = std::get<TaskClearLine>(task.task);
				S2C_CLEARLINE_PACKET clear_line_p;
				clear_line_p.header.size = static_cast<std::uint16_t>(sizeof(clear_line_p));
				clear_line_p.header.type = S2C_CLEARLINE;
				clear_line_p.id = user_id;
				clear_line_p.score = r_user.GetScore();
				clear_line_p.line_index = t.line_index;
				clear_line_p.combo = r_user.GetCombo();
				if (!r_user.AddToTickBuffer(reinterpret_cast<char*>(&clear_line_p), clear_line_p.header.size)) return user_id;

				break;
			}
			case EVENT_TYPE::SPAWN: {
				if (r_user.GetTetrominoIndex() == (tetromino_spawn_list.size() - 2)) Add7BagTetrominoList();
				r_user.AddTetrominoIndex();

				if (SpawnTetromino(user_id)) {
					S2C_SPAWN_PACKET spawn_p;
					spawn_p.header.size = static_cast<std::uint16_t>(sizeof(spawn_p));
					spawn_p.header.type = S2C_SPAWN;
					spawn_p.id = user_id;
					spawn_p.tetromino_type = tetromino_spawn_list[r_user.GetTetrominoIndex()];
					spawn_p.next_tetromino_type = tetromino_spawn_list[r_user.GetTetrominoIndex() + 1];
					spawn_p.spawn_x = static_cast<char>(spawn_pos.x);
					spawn_p.spawn_y = static_cast<char>(spawn_pos.y);
					if (!r_user.AddToTickBuffer(reinterpret_cast<char*>(&spawn_p), spawn_p.header.size)) return user_id;
				}
				break;
			}

			case EVENT_TYPE::ADDLINE: {
				r_user.GetTetris().GetTickData().SetGarbageLineTick(0);
				S2C_ADDLINE_PACKET add_line_p;
				add_line_p.header.size = static_cast<std::uint16_t>(sizeof(add_line_p));
				add_line_p.header.type = S2C_ADDLINE;
				add_line_p.id = user_id;
				auto& t = std::get<TaskAddLine>(task.task);
				add_line_p.hole_x = static_cast<char>(t.hole_x);
				if (!r_user.AddToTickBuffer(reinterpret_cast<char*>(&add_line_p), add_line_p.header.size)) return user_id;
				break;
			}

			case EVENT_TYPE::GAMEOVER: {
				// 일단 종료 패킷을 보냄
				r_user.SetRoomUserState(ROOM_USER_STATE::GAMEOVER);
				S2C_GAMEOVER_PACKET gameover_p;
				gameover_p.header.size = static_cast<std::uint16_t>(sizeof(gameover_p));
				gameover_p.header.type = S2C_GAMEOVER;
				gameover_p.id = user_id;
				if (!r_user.AddToTickBuffer(reinterpret_cast<char*>(&gameover_p), gameover_p.header.size)) return user_id;

				break;
			}

			case EVENT_TYPE::GAMEEND: { // 이건 사실상 멀티만 쓰므로.. 근데 이거 하나때문에 또 분리하기 좀 그렇긴 하다 분리하는게 좋긴 할 것 같지만..
				auto& t = std::get<TaskGameEnd>(task.task);

				S2C_GAMEEND_PACKET gameend_p;
				gameend_p.header.size = static_cast<std::uint16_t>(sizeof(gameend_p));
				gameend_p.header.type = S2C_GAMEEND;
				gameend_p.winner_id = t.winner_id;
				if (!r_user.AddToTickBuffer(reinterpret_cast<char*>(&gameend_p), gameend_p.header.size)) return user_id;
				break;
			}
			}
		}
	}
	return -1;
}

int TetrisRoom::BoundAllPackets()
{
	for (auto& target : room_users) {
		auto target_session = target.GetSession();
		if (!target_session) continue;
		int target_user_id = target_session->GetDBInfo().id;

		for (auto& source : room_users) {
			auto source_session = source.GetSession(); 
			if (!source_session) continue; // 객체 내에 세션이 살아 있는지 확인
			int data_size = source.GetTickDataSize();
			if (data_size <= 0) continue; // 보낼 데이터가 있는지 확인
			if (!target.AddToSendBuffer(source.GetTickBuf(), data_size)) return target_user_id;
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
	return r_session.AddToTickBuffer(reinterpret_cast<char*>(&move_p), move_p.header.size);
}

void TetrisRoom::AddGarbageLines()
{
	for (auto& r_user : room_users) {
		if (r_user.GetRoomUserState() == ROOM_USER_STATE::PLAY) {
			r_user.GetTetris().AddGarbageLines();
		}
	}
}

void TetrisRoom::AddSpawnTask()
{
	// spawn은 룸에서 이루어져야 한다. 스폰될 테트로미노를 일괄 관리중이기 때문이다.
	// spawn은 fix와 항상 같이 일어나므로, 작업에서 fix 여부를 확인해 있으면 추가해준다.
	for (auto& r_user : room_users) {
		if (r_user.GetRoomUserState() != ROOM_USER_STATE::PLAY) continue;
		std::vector<TaskType>& tasks = r_user.GetTetris().GetSendTasks();
		bool fix_exists = false;
		for (auto& task : tasks) {
			if (task.event_type == EVENT_TYPE::FIX) {
				fix_exists = true;
				break;
			}
		}

		if (fix_exists) {
			TaskType t_type;
			t_type.event_type = EVENT_TYPE::SPAWN;
			r_user.GetTetris().GetSendTasks().emplace_back(t_type);
		}
	}
}

void TetrisRoom::ResetUsersTickData()
{
	for(auto& r_user : room_users){
		auto session = r_user.GetSession();
		if (!session) continue;
		r_user.GetTetris().ResetTickData();
	}
}

void TetrisRoom::BroadcastTickDataForUsers()
{
	for (auto& r_user : room_users) {
		auto session = r_user.GetSession();
		if (!session) continue;
		//IOKey key = source.GetSession()->GetIOKey();
		char* send_buffer = r_user.GetSendBuf();
		int data_size = r_user.GetSendDataSize();
		// data_size = 0이 IOCP로 들어가면 연결이 종료되는 것에 주의해야함
		// 게임 시작 시 틱마다 자동 전송하므로 조건이 꼭 필요
		if (data_size > 0) {
			session->SendBoundPacket(send_buffer, data_size, server->GetHandle());
		}
	}

	for (auto& r_user : room_users) {
		auto session = r_user.GetSession();
		if (!session) continue;
		r_user.ClearBuffers();
	}
}

void TetrisRoom::Broadcast(char* packet, const HANDLE iocp_handle)
{
	// 범위기반을 있다고 생각하고 만들었는데, 그 순회하는 사이에 접근하기 전에 삭제되면 세션 포인터 nullptr 오류가 생긴다.
	// 결국 락을 걸 수밖에..
	// 틱 루프에서 호출할 경우 이중락 걸리므로 주의
	// 우선 외부에서 잠그는걸로 다시 변경. 범위기반 탐색과 삭제 사이의 관계 때문에 락이 필요한데, 멀티와 같은 경우 범위기반 탐색 내에 다시 범위기반 탐색을 하는 경우도 꽤 있으므로 외부에서 하는게 효율적인 것 같다.
	std::vector<int> target_index;
	for (auto& r_user : room_users) {
		auto session = r_user.GetSession();
		if (!session) continue;
		session->SendPacket(packet, iocp_handle);
	}
}

void TetrisRoom::SetRoomIndex(const int val)
{
	room_index = val;
}

void TetrisRoom::SetRoomGen(const int val)
{
	room_gen = val;
}

void TetrisRoom::StoreRoomState(ROOM_STATE new_state)
{
	room_state.Store(new_state);
}

bool TetrisRoom::TryChangeRoomState(ROOM_STATE expected, ROOM_STATE desired)
{
	return room_state.Compare_exchange_strong(expected, desired);
}
