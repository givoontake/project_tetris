#include <random>
#include "SingleRoom.h"

SingleRoom::SingleRoom(IOCPServer* server, Session* session, char max_user, char room_name[MAX_ROOM_NAME], char room_password[MAX_ROOM_PASSWORD])
	: TetrisRoom(server, session, max_user, room_name, room_password)
{
}

SingleRoom::SingleRoom(IOCPServer* server, Session* session, char max_user, char room_name[MAX_ROOM_NAME])
	: TetrisRoom(server, session, max_user, room_name)
{
}

void SingleRoom::HandlePacket(char* packet, Session* request_session)
{
	std::cout << "SingleRoom::HandlePacket, Packet type: ";
	PrintPacketType(packet[2]);

	switch (packet[2]) {

	case C2S_START: {
		StartGame();
		break;
	}

	case C2S_DELETE_USER: {
		DeleteUser(request_session->GetId());
		break;
	}

	case C2S_MOVE: {
		C2S_MOVE_PACKET* recv_p = reinterpret_cast<C2S_MOVE_PACKET*>(packet);
		TaskInfo new_task;
		new_task.id = room_users[0].GetSession()->GetId();
		new_task.type = static_cast<EVENT_TYPE>(recv_p->move_type);
		GetTasks().AddTask(new_task);
		break;
	}
	}
}

void SingleRoom::StartGame()
{
	if (room_state == ROOM_STATE::PLAY) return;

	SetRoomState(ROOM_STATE::PLAY);

	// 모든 조건 통과->게임 시작
	Add7BagTetrominoList();
	for (auto& r_user : room_users) {
		if (r_user.GetRoomUserState() == ROOM_USER_STATE::EMPTY) continue;
		r_user.SetRoomUserState(ROOM_USER_STATE::PLAY);
		r_user.GetTetris().InitNewTetromino(tetromino_spawn_list[r_user.GetTetrominoIndex()], spawn_pos);
	}

	// 테트리스 게임 중에 들어오는 패킷은 또 따로 분리하고 싶기는 한데..
	InitGame();

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
		spawn_p.id = r_user.GetSession()->GetId();
		spawn_p.tetromino_type = tetromino_spawn_list[r_user.GetTetrominoIndex()];
		spawn_p.next_tetromino_type = tetromino_spawn_list[r_user.GetTetrominoIndex() + 1];
		spawn_p.spawn_x = spawn_pos.x;
		spawn_p.spawn_y = spawn_pos.y;
		Broadcast(reinterpret_cast<char*>(&spawn_p), server->GetHandle());
	}
}

void SingleRoom::BoundPackets(RoomSession& r_session, std::vector<TaskType>& tasks)
{
	for (auto& task : tasks) {
		switch (task.event_type) {
		case EVENT_TYPE::MOVE: {
			// 각 무브별 틱 초기화 추가가 애매하므로, 무브 틱 값 초기화는 Tetris::HandleTetrominoKeyInput에서 처리
			auto& t = std::get<TaskMove>(task.task);
			MakeMovePacketData(r_session, static_cast<int>(t.move_type));
			break;
		}

		// fix는 항상 라인 클리어와 스폰을 동반한다.
		case EVENT_TYPE::FIX: {
			r_session.GetTetris().FixTetromino();
			r_session.GetTetris().GetTickData().SetDownTick(0);
			r_session.GetTetris().GetTickData().SetDownTimeoutTick(0);
			r_session.GetTetris().GetTickData().SetDropTick(0);

			S2C_FIX_PACKET fix_p;
			fix_p.size = sizeof(S2C_FIX_PACKET);
			fix_p.type = S2C_FIX;
			fix_p.id = r_session.GetSession()->GetId();
			fix_p.fixed_x = r_session.GetTetris().GetCurrentTetromino().moved_pos.x;
			fix_p.fixed_y = r_session.GetTetris().GetCurrentTetromino().moved_pos.y;
			r_session.AddToSendBuffer(reinterpret_cast<char*>(&fix_p), fix_p.size);

			// 줄 추가시 게임 오버가 될 수도 있지만 블록 고정시에도 게임 오버가 될 수 있다. 이것 역시 fix와 동반되는 과정이다.
			if (r_session.GetTetris().CheckGameover()) {
				// 일단 종료 패킷을 보냄
				r_session.SetRoomUserState(ROOM_USER_STATE::WAIT);
				S2C_GAMEOVER_PACKET send_p; // 싱글은 게임오버 = 게임 끝
				send_p.size = sizeof(S2C_GAMEOVER_PACKET);
				send_p.type = S2C_GAMEOVER;
				send_p.id = r_session.GetSession()->GetId();
				r_session.AddToSendBuffer(reinterpret_cast<char*>(&send_p), send_p.size);
				r_session.GetSession()->SendBoundPacket(reinterpret_cast<char*>(&send_p), send_p.size, server->GetHandle());
				RequestUpdateScore(r_session);
				ClearGame();
				return;
			}

			std::vector<char> index_lines = r_session.GetTetris().ClearLine();
			CalculateScore(r_session, index_lines.size());
			if (!index_lines.empty()) {
				//r_session.SetScore(r_session.GetScore() + (CLEAR_LINE_SCORE * index_lines.size() * index_lines.size()));
				for (int i = 0; i < index_lines.size(); i++) {
					S2C_CLEARLINE_PACKET clear_line_p;
					clear_line_p.size = sizeof(S2C_CLEARLINE_PACKET);
					clear_line_p.type = S2C_CLEARLINE;
					clear_line_p.id = r_session.GetSession()->GetId();
					clear_line_p.score = r_session.GetScore();
					clear_line_p.line_index = index_lines[i];
					clear_line_p.combo = r_session.GetCombo();
					r_session.AddToSendBuffer(reinterpret_cast<char*>(&clear_line_p), clear_line_p.size);
				}
			}

			if (r_session.GetTetrominoIndex() == tetromino_spawn_list.size() - 2) Add7BagTetrominoList();
			r_session.AddTetrominoIndex();

			if (SetNewTetromino(r_session.GetSession()->GetId())) {
				S2C_SPAWN_PACKET spawn_p;
				spawn_p.size = sizeof(S2C_SPAWN_PACKET);
				spawn_p.type = S2C_SPAWN;
				spawn_p.id = r_session.GetSession()->GetId();
				spawn_p.tetromino_type = tetromino_spawn_list[r_session.GetTetrominoIndex()];
				spawn_p.next_tetromino_type = tetromino_spawn_list[r_session.GetTetrominoIndex() + 1];
				spawn_p.spawn_x = static_cast<char>(spawn_pos.x);
				spawn_p.spawn_y = static_cast<char>(spawn_pos.y);
				r_session.AddToSendBuffer(reinterpret_cast<char*>(&spawn_p), spawn_p.size);
			}
			break;
		}

		case EVENT_TYPE::ADDLINE: {
			r_session.GetTetris().GetTickData().SetGarbageLineTick(0);
			S2C_ADDLINE_PACKET add_line_p;
			add_line_p.size = sizeof(S2C_ADDLINE_PACKET);
			add_line_p.type = S2C_ADDLINE;
			add_line_p.id = r_session.GetSession()->GetId();
			auto& t = std::get<TaskAddLine>(task.task);
			add_line_p.hole_x = static_cast<char>(t.hole_x);
			r_session.AddToSendBuffer(reinterpret_cast<char*>(&add_line_p), add_line_p.size);
			break;
		}

		case EVENT_TYPE::GAMEOVER: {
			// 일단 종료 패킷을 보냄
			r_session.SetRoomUserState(ROOM_USER_STATE::WAIT);
			S2C_GAMEOVER_PACKET send_p; // 싱글은 게임오버 = 게임 끝
			send_p.size = sizeof(S2C_GAMEOVER_PACKET);
			send_p.type = S2C_GAMEOVER;
			send_p.id = r_session.GetSession()->GetId();
			r_session.AddToSendBuffer(reinterpret_cast<char*>(&send_p), send_p.size);
			r_session.GetSession()->SendBoundPacket(reinterpret_cast<char*>(&send_p), send_p.size, server->GetHandle());
			RequestUpdateScore(r_session);
			ClearGame();
			return;
		}
		}
	}
}

void SingleRoom::DeleteUser(const int id)
{
	//std::cout << "delete user id: " << id << std::endl;
	for (auto& r_user : room_users) {
		if (r_user.GetRoomUserState() == ROOM_USER_STATE::EMPTY) continue;
		if (r_user.GetSession()->GetId() == id) { // 삭제할 아이디 검색
			//std::cout << "delete user id: " << id << std::endl;
			//room_mutex.lock();
			r_user.GetSession()->SetState(USER_STATE::LOBBY);

			S2C_DELETE_USER_PACKET p;
			p.size = sizeof(S2C_DELETE_USER_PACKET);
			p.type = S2C_DELETE_USER;
			p.id = id;
			//room_mutex.unlock();
			Broadcast(reinterpret_cast<char*>(&p), server->GetHandle());
			room_state.Store(ROOM_STATE::WAITING_DELETE);
			ExOverlapped* delete_over = new ExOverlapped;
			delete_over->op_type = OP_TYPE::DELETE_ROOM;
			PostQueuedCompletionStatus(server->GetHandle(), 1, room_index, reinterpret_cast<WSAOVERLAPPED*>(delete_over));
		}
	}
}

void SingleRoom::ReduceTimeouts(int type, RoomSession& r_session)
{
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

void SingleRoom::CalculateScore(RoomSession& r_session, int clear_line_count)
{
	int added_score = 0;
	switch (clear_line_count) {
	case 0:
		r_session.SetCombo(0);
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
	int combo = r_session.GetCombo();
	r_session.SetCombo(combo + 1);
	added_score += combo * (added_score / 10);
	r_session.SetScore(r_session.GetScore() + added_score);
}

void SingleRoom::MakeMovePacketData(RoomSession& r_session, int move_type)
{
	S2C_MOVE_PACKET move_p;
	move_p.size = sizeof(S2C_MOVE_PACKET);
	move_p.type = S2C_MOVE;
	move_p.id = r_session.GetSession()->GetId();
	move_p.move_type = static_cast<char>(move_type);
	r_session.AddToSendBuffer(reinterpret_cast<char*>(&move_p), move_p.size);
}

void SingleRoom::ClearEventsInTick()
{
	for (auto& r_user : room_users) {
		if (r_user.GetRoomUserState() == ROOM_USER_STATE::EMPTY) continue;
		r_user.GetTetris().ClearPendingMoves();
		r_user.GetTetris().ClearTasks();
	}
}

void SingleRoom::RequestUpdateScore(RoomSession& r_session)
{
	if (r_session.GetSession()->GetInfo().max_score < r_session.GetScore()) {
		int id = r_session.GetSession()->GetId();
		int index = r_session.GetSession()->GetIndex();
		std::string login_id = r_session.GetSession()->GetInfo().login_id;
		int new_score = r_session.GetScore();
		Database& db = server->GetDB();
		auto task_update_score = [id, index, login_id, new_score, &db] {
			db.ExecuteUpdateScore(id, index, login_id, new_score);
			};
		server->GetDB().Enqueue(task_update_score);
	}
}

