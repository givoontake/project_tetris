#include <random>
#include <algorithm>
#include "SingleRoom.h"
#include "packet_type.h"

SingleRoom::SingleRoom(IOCPServer* server, Session& session, OpenRoomInitData data)
	: TetrisRoom(server, session, data)
{
}

SingleRoom::SingleRoom(IOCPServer* server, Session& session, LockRoomInitData data)
	: TetrisRoom(server, session, data)
{
}

void SingleRoom::HandlePacket(char* packet, Session& request_session) 
{
	//std::cout << "SingleRoom::HandlePacket, Packet type: ";
	//PrintPacketType(packet[2]);

	switch (packet[2]) {

	case C2S_START: {
		StartGame();
		break;
	}

	case C2S_DELETE_USER: {
		DeleteUser(request_session.GetSessionKey().id);
		break;
	}

	case C2S_MOVE: {
		C2S_MOVE_PACKET* recv_p = reinterpret_cast<C2S_MOVE_PACKET*>(packet);
		TaskInfo new_task;
		new_task.id = room_users[0].GetSession()->GetSessionKey().id;
		new_task.type = static_cast<EVENT_TYPE>(recv_p->move_type);
		GetTasks().AddTask(new_task);
		break;
	}

	case C2S_GIVEUP: {
		std::lock_guard<std::mutex> lock(room_mutex);

		if (room_state == ROOM_STATE::PLAY) {
			S2C_GAMEOVER_PACKET gameover_p;
			gameover_p.size = sizeof(S2C_GAMEOVER_PACKET);
			gameover_p.type = S2C_GAMEOVER;
			gameover_p.id = room_users[0].GetSession()->GetSessionKey().id;
			room_users[0].GetSession()->SendPacket(reinterpret_cast<char*>(&gameover_p), server->GetHandle());
			RequestUpdateScore();
			ClearGame();
		}
	}
	}
}

void SingleRoom::ProcessPlayTasks()
{
	{
		// delete가 도중에 일어나면 문제가 되는 일이 많아진다.
		// 싱글에서 유저 제거는 방 삭제를 동반한다. 클리어 작업 후 [0]에 접근할 수도 있고, 범위기반 반복문 진입 전에는 있었는데 막상 실행할 때는 없을 수도 있음. 즉 처음에 잡은 범위 스코프 메모리를 무효화된다.
		// 따라서 삭제는 틱 처리 도중에 일어나서는 안된다.
		std::lock_guard<std::mutex> lock(room_mutex);
		if (room_state.Load() != ROOM_STATE::PLAY) return;
		tasks.SwapTask();
		while (!tasks.task_queue.IsEmpty()) {
			TaskInfo task = tasks.GetTask();
			for (auto& r_user : room_users) {
				if (r_user.GetRoomUserState() == ROOM_USER_STATE::EMPTY) continue;
				if (r_user.GetSession()->GetSessionKey().id == task.id) {
					// 각 작업들을 각 세션에 분배
					r_user.GetTetris().GetInputTasks().emplace_back(task.type);

					//r_user.GetTetris().DebugPrintBoard();
					break;
				}
			}
		}

		for (auto& r_user : room_users) {
			if (r_user.GetRoomUserState() == ROOM_USER_STATE::EMPTY) continue;
			r_user.GetTetris().TickProcess();
		}

		AddGarbageLines();
		bool is_over = false;
		is_over = room_users[0].GetTetris().CheckGameover();
		if (!is_over) AddSpawnTask();

		auto tasks = room_users[0].GetTetris().GetSendTasks();
		auto it = std::find_if(tasks.begin(), tasks.end(), [](const TaskType& t_type) {
			return t_type.event_type == EVENT_TYPE::FIX;
			});

		if (it != tasks.end()) { // 고정 이벤트가 있어야 점수 및 콤보계산
			CalculateScore(room_users[0].GetTetris().GetClearedLines());
		}
		BoundPackets();

		ResetUsersTickData();
		BroadcastTickDataForUsers();
		if (is_over) {
			RequestUpdateScore();
			ClearGame();
		}
	}
}

void SingleRoom::StartGame()
{
	{
		std::lock_guard<std::mutex> lock(room_mutex);
		if (room_state == ROOM_STATE::PLAY) return;
		if (!TryChangeRoomState(ROOM_STATE::WAIT, ROOM_STATE::PLAY)) return;

		// 모든 조건 통과->게임 시작
		InitGame();
		Add7BagTetrominoList();
		for (auto& r_user : room_users) {
			if (r_user.GetRoomUserState() == ROOM_USER_STATE::EMPTY) continue;
			r_user.SetRoomUserState(ROOM_USER_STATE::PLAY);
			r_user.GetTetris().InitNewTetromino(tetromino_spawn_list[r_user.GetTetrominoIndex()], spawn_pos);
		}

		S2C_SINGLE_START_PACKET start_p;
		start_p.size = sizeof(S2C_SINGLE_START_PACKET);
		start_p.type = S2C_SINGLE_START;
		start_p.score = 0;
		Broadcast(reinterpret_cast<char*>(&start_p), server->GetHandle());

		for (auto& r_user : room_users) {
			if (r_user.GetRoomUserState() == ROOM_USER_STATE::EMPTY) continue;
			S2C_SPAWN_PACKET spawn_p;
			spawn_p.size = sizeof(S2C_SPAWN_PACKET);
			spawn_p.type = S2C_SPAWN;
			spawn_p.id = r_user.GetSession()->GetSessionKey().id;
			spawn_p.tetromino_type = tetromino_spawn_list[r_user.GetTetrominoIndex()];
			spawn_p.next_tetromino_type = tetromino_spawn_list[r_user.GetTetrominoIndex() + 1];
			spawn_p.spawn_x = spawn_pos.x;
			spawn_p.spawn_y = spawn_pos.y;
			Broadcast(reinterpret_cast<char*>(&spawn_p), server->GetHandle());
		}
	}
}

void SingleRoom::DeleteUser(const int id)
{
	//std::cout << "delete user id: " << id << std::endl;

	{
		// 삭제 처리와 틱 시작 처리는 락으로 동기화, 
		std::lock_guard<std::mutex> lock(room_mutex);
		for (auto& r_user : room_users) {
			if (r_user.GetRoomUserState() == ROOM_USER_STATE::EMPTY) continue;
			if (r_user.GetSession()->GetSessionKey().id == id) { // 삭제할 아이디 검색
				//std::cout << "delete user id: " << id << std::endl;
				//room_mutex.lock();
				r_user.GetSession()->StoreState(SESS_STATE::LOBBY);
				S2C_DELETE_USER_PACKET p;
				p.size = sizeof(S2C_DELETE_USER_PACKET);
				p.type = S2C_DELETE_USER;
				p.id = id;

				Broadcast(reinterpret_cast<char*>(&p), server->GetHandle());

				ClearRoom(); // 안하면 방 삭제 포스트 이후 세션이 재사용되면 문제가 될 수 있음.
				room_state.Store(ROOM_STATE::WAITING_DELETE);
				ExOverlapped* delete_over = new ExOverlapped;
				delete_over->op_type = OP_TYPE::DELETE_ROOM;
				PostQueuedCompletionStatus(server->GetHandle(), 1, room_index, reinterpret_cast<WSAOVERLAPPED*>(delete_over));
				break;
			}
		}
	}
}

void SingleRoom::SendCreateRoom(Session& session) // 외부에서 세션락 걸고 들어온다
{
	if (!room_password) {
		S2C_ADD_OPEN_ROOM_PACKET open_p;
		open_p.size = sizeof(S2C_ADD_OPEN_ROOM_PACKET);
		open_p.type = S2C_ADD_OPEN_ROOM;
		open_p.id = session.GetSessionKey().id;
		open_p.max_user = max_user;
		memcpy(open_p.room_name, room_name, sizeof(room_name));
		session.SendPacket(reinterpret_cast<char*>(&open_p), server->GetHandle());
	}
	else {
		S2C_ADD_LOCK_ROOM_PACKET lock_p;
		lock_p.size = sizeof(S2C_ADD_LOCK_ROOM_PACKET);
		lock_p.type = S2C_ADD_LOCK_ROOM;
		lock_p.id = session.GetSessionKey().id;
		lock_p.max_user = max_user;
		memcpy(lock_p.room_name, room_name, sizeof(room_name));
		memcpy(lock_p.room_password, room_password, MAX_ROOM_PASSWORD);
		session.SendPacket(reinterpret_cast<char*>(&lock_p), server->GetHandle());
	}
	std::cout << "Room[: " << room_index << "] created by : " << session.GetDBInfo().nickname << "\n";
}

void SingleRoom::ReduceTimeouts(int type)
{
	RoomSession r_session = room_users[0];
	switch (type) {
	case DOWN_TIMEOUT:
		if (r_session.GetScore() <= 100) {
			r_session.GetTetris().GetTickData().SetDownTimeout(25);
		}

		else if (100 < r_session.GetScore() && r_session.GetScore() <= 300) {
			r_session.GetTetris().GetTickData().SetDownTimeout(24);
		}

		else if (300 < r_session.GetScore() && r_session.GetScore() <= 600) {
			r_session.GetTetris().GetTickData().SetDownTimeout(23);
		}

		else if (600 < r_session.GetScore() && r_session.GetScore() <= 1000) {
			r_session.GetTetris().GetTickData().SetDownTimeout(22);
		}

		else if (1000 < r_session.GetScore() && r_session.GetScore() <= 1500) {
			r_session.GetTetris().GetTickData().SetDownTimeout(21);
		}

		else if (1500 < r_session.GetScore()) {
			r_session.GetTetris().GetTickData().SetDownTimeout(20);
		}

		break;

	case ADD_TIMEOUT:
		if (r_session.GetTetris().GetTickData().GetGarbageLineTimeout() > 500)
			r_session.GetTetris().GetTickData().SetGarbageLineTimeout(r_session.GetTetris().GetTickData().GetGarbageLineTimeout() - 5);
		break;

	default:
		break;
	}
}

void SingleRoom::CalculateScore(int clear_line_count)
{
	int added_score = 0;
	switch (clear_line_count) {
	case 0:
		room_users[0].ResetCombo();
		return;

		break;
	case 1:
		added_score = 100;
		break;
	case 2:
		added_score = 300;
		break;
	case 3:
		added_score = 500;
		break;
	case 4:
		added_score = 800;
		break;
	default:
		break;
	}
	room_users[0].AddCombo();
	int combo = room_users[0].GetCombo();
	added_score += combo * (added_score / 10);
	room_users[0].AddScore(added_score);
}

void SingleRoom::MakeMovePacketData(int move_type)
{
	S2C_MOVE_PACKET move_p;
	move_p.size = sizeof(S2C_MOVE_PACKET);
	move_p.type = S2C_MOVE;
	move_p.id = room_users[0].GetSession()->GetSessionKey().id;
	move_p.move_type = static_cast<char>(move_type);
	room_users[0].AddToSendBuffer(reinterpret_cast<char*>(&move_p), move_p.size);
}

void SingleRoom::RequestUpdateScore()
{
	if (room_users[0].GetSession()->GetDBInfo().max_score < room_users[0].GetScore()) {
		SessionKey key;
		key.id = room_users[0].GetSession()->GetSessionKey().id;
		key.index = room_users[0].GetSession()->GetSessionKey().index;
		int db_PK = room_users[0].GetSession()->GetDBInfo().db_pk;
		int new_score = room_users[0].GetScore();
		Database& db = server->GetDB();
		auto task_update_score = [key, db_PK, new_score, &db] {
			db.ExecuteUpdateScore(key, db_PK, new_score);
			};
		server->GetDB().Enqueue(task_update_score);
	}
}
