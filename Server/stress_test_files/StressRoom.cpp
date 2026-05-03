#include "StressRoom.h"

StressRoom::StressRoom(IOCPServer* server, OpenRoomInitData data)
	: MultiRoom(server, data)
{
}

StressRoom::StressRoom(IOCPServer* server, LockRoomInitData data)
	: MultiRoom(server, data)
{
}

StressRoom::~StressRoom()
{
}

void StressRoom::StartStressGame()
{
	std::lock_guard<std::mutex> lock(room_mutex);
	if (room_state.Load() == ROOM_STATE::PLAY) return;
	if (GetCurrentUser() != GetMaxUser()) return;
	if (!TryChangeRoomState(ROOM_STATE::WAIT, ROOM_STATE::PLAY)) return;

	S2C_MULTI_START_PACKET start_p;
	start_p.header.size = static_cast<std::uint16_t>(sizeof(start_p));
	start_p.header.type = S2C_MULTI_START;
	Broadcast(reinterpret_cast<char*>(&start_p), server->GetHandle());

	Add7BagTetrominoList();
	for (auto& r_user : room_users) {
		auto session = r_user.GetSession();
		if (!session) continue;
		r_user.SetRoomUserState(ROOM_USER_STATE::PLAY);
		r_user.GetTetris().InitNewTetromino(tetromino_spawn_list[r_user.GetTetrominoIndex()], spawn_pos);
	}

	InitGame();

	for (auto& r_user : room_users) {
		auto session = r_user.GetSession();
		if (!session) continue;
		S2C_SPAWN_PACKET spawn_p;
		spawn_p.header.size = static_cast<std::uint16_t>(sizeof(spawn_p));
		spawn_p.header.type = S2C_SPAWN;
		spawn_p.id = session->GetDBInfo().id;
		spawn_p.tetromino_type = tetromino_spawn_list[r_user.GetTetrominoIndex()];
		spawn_p.next_tetromino_type = tetromino_spawn_list[r_user.GetTetrominoIndex() + 1];
		spawn_p.spawn_x = spawn_pos.x;
		spawn_p.spawn_y = spawn_pos.y;
		Broadcast(reinterpret_cast<char*>(&spawn_p), server->GetHandle());
	}
}

void StressRoom::ProcessPlayTasks()
{
	std::unique_lock<std::mutex> lock(room_mutex);
	if (room_state.Load() != ROOM_STATE::PLAY) return;
	UpdateTick();
	tasks.SwapTask();
	int failed_user_id = -1;
	while (!tasks.task_queue.IsEmpty()) {
		TaskInfo task = tasks.GetTask();
		for (auto& r_user : room_users) {
			auto session = r_user.GetSession();
			if (!session) continue;
			if (session->GetDBInfo().id == task.id) {
				r_user.GetTetris().GetInputTasks().emplace_back(task.type);
				if (task.is_test) {
					S2C_TEST_MOVE_PACKET send_p;
					send_p.header.size = static_cast<std::uint16_t>(sizeof(send_p));
					send_p.header.type = S2C_TEST_MOVE;
					send_p.id = task.id;
					send_p.sequence = task.sequence;
					send_p.client_time = task.client_time;
					if (!r_user.AddToSendBuffer(reinterpret_cast<char*>(&send_p), send_p.header.size)) {
						failed_user_id = task.id;
					}
				}
				break;
			}
		}
		if (failed_user_id != -1) break;
	}
	if (failed_user_id != -1) {
		lock.unlock();
		DeleteUser(failed_user_id);
		return;
	}
	UpdatePrevUsersState();

	for (auto& r_user : room_users) {
		auto session = r_user.GetSession();
		if (!session) continue;
		r_user.GetTetris().TickProcess();
	}

	CalcAttackLine();
	AddGarbageLines();
	bool game_end = false;
	game_end = FindWinner();
	if (!game_end) AddSpawnTask();
	failed_user_id = BoundPackets();
	if (failed_user_id != -1) {
		lock.unlock();
		DeleteUser(failed_user_id);
		return;
	}

	ResetUsersTickData();
	BroadcastTickDataForUsers();
	if (game_end) {
		ClearGame();
		lock.unlock();
		StartStressGame();
	}
}
