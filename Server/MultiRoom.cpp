#include "MultiRoom.h"

MultiRoom::MultiRoom(IOCPServer* server, Session* session, char max_user, char room_name[MAX_ROOM_NAME], char room_password[MAX_ROOM_PASSWORD])
	: TetrisRoom(server, session, max_user, room_name, room_password)
{
}

MultiRoom::MultiRoom(IOCPServer* server, Session* session, char max_user, char room_name[MAX_ROOM_NAME])
	: TetrisRoom(server, session, max_user, room_name)
{
}

MultiRoom::~MultiRoom()
{
}

void MultiRoom::HandlePacket(char* packet, Session* request_session)
{
	switch (packet[2]) {

	case C2S_ADD_USER: {
		C2S_ADD_USER_PACKET* add_p = reinterpret_cast<C2S_ADD_USER_PACKET*>(packet);
		AddUser(request_session);
		break;
	}

	case C2S_DELETE_USER: {
		C2S_DELETE_USER_PACKET* delete_p = reinterpret_cast<C2S_DELETE_USER_PACKET*>(packet);
		DeleteUser(delete_p->id);
		break;
	}

	case C2S_READY: {
		C2S_READY_PACKET* ready_p = reinterpret_cast<C2S_READY_PACKET*>(packet);
		ReadyUser(ready_p->id);
		break;
	}

	case C2S_KICK: {
		C2S_KICK_PACKET* kick_p = reinterpret_cast<C2S_KICK_PACKET*>(packet);
		KickUser(request_session->GetId(), kick_p->kick_user_id);
		break;
	}

	case C2S_START: {
		//C2S_START_PACKET* recv_p = reinterpret_cast<C2S_START_PACKET*>(packet);
		StartGame(request_session->GetId());
		break;
	}

	case C2S_MOVE: {
		C2S_MOVE_PACKET* recv_p = reinterpret_cast<C2S_MOVE_PACKET*>(packet);
		TaskInfo new_task;
		new_task.id = request_session->GetId();
		new_task.type = static_cast<EVENT_TYPE>(recv_p->move_type);
		GetTasks().AddTask(new_task);
		break;
	}
	}
}


void MultiRoom::AddUser(Session* new_session)
{
	//C2S_ADD_USER_PACKET* recv_p = reinterpret_cast<C2S_ADD_USER_PACKET*>(packet);

	bool b_send = false;
	for (auto& r_user : room_users) {
		if (r_user.GetRoomUserState() != ROOM_USER_STATE::EMPTY) continue;

		else {
			new_session->SetState(USER_STATE::ROOM);
			r_user.InitSession(new_session);
			S2C_ADD_USER_PACKET p;
			p.size = sizeof(S2C_ADD_USER_PACKET);
			p.type = S2C_ADD_USER;
			// p.name = 세션에 이름 변수 추가 필요
			p.id = new_session->GetId();
			p.is_add = true;
			Broadcast(reinterpret_cast<char*>(&p), server->GetHandle());

			return;
		}

	}

	// 성공과 실패에 따라 패킷을 나눌까? 사실 방이 다 차 있다면 클라이언트 수준에서 송신 자체를 막아야 할 것 같기는 한데..
	// 아니지. 클라에서 실제로 방이 빈 것으로 보였어도, 누군가가 먼저 차지했다면 그건 알 수가 없으니까 처리가 필요
	S2C_ADD_USER_PACKET p;
	p.size = sizeof(S2C_ADD_USER_PACKET);
	p.type = S2C_ADD_USER;
	// p.name = 세션에 이름 변수 추가 필요
	p.id = new_session->GetId();
	p.is_add = false;
	new_session->SendPacket(reinterpret_cast<char*>(&p), server->GetHandle()); // 방이 꽉 찼을 경우 본인에게만 실패 전송
}

void MultiRoom::DeleteUser(const int id)
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
			int new_host_id = -1;
			if (p.id == host_id) { // 새 방장 여부에 따른 처리
				new_host_id = FindNewHost(p.id);
				host_id = new_host_id;
				p.new_host_id = new_host_id;
			}
			Broadcast(reinterpret_cast<char*>(&p), server->GetHandle());
			if (new_host_id == -1) { // 현재 방에 아무도 없으면
				room_state.Store(ROOM_STATE::WAITING_DELETE);
				PostQueuedCompletionStatus(server->GetHandle(), 1, room_index, nullptr);
			}
			r_user.ClearSession(); // 해당 아이디 세션 정리

			break;
			//room_mutex.unlock();
		}
	}
}

void MultiRoom::ReadyUser(int id)
{
	if (host_id == id) return;

	for (auto& r_user : room_users) {
		if (r_user.GetSession()->GetId() == id) { // 레디 상태 변화
			r_user.SetRoomUserState(ROOM_USER_STATE::READY);

			S2C_READY_PACKET p;
			p.size = sizeof(S2C_READY_PACKET);
			p.type = S2C_READY;
			p.id = r_user.GetSession()->GetId();
			if (r_user.GetRoomUserState() == ROOM_USER_STATE::READY) p.is_ready = true;
			else p.is_ready = false;

			Broadcast(reinterpret_cast<char*>(&p), server->GetHandle());
			break;
		}
	}

}

void MultiRoom::KickUser(int id, int kick_user_id)
{
	if (id != host_id) return;

	for (auto& r_user : room_users) {
		if (r_user.GetSession()->GetId() == kick_user_id) { // 삭제할 아이디 검색
			r_user.ClearSession(); // 해당 아이디 세션 정리
			r_user.SetRoomUserState(ROOM_USER_STATE::EMPTY);
			r_user.GetSession()->SetState(USER_STATE::LOBBY);

			S2C_KICK_PACKET p;
			p.size = sizeof(S2C_KICK_PACKET);
			p.type = S2C_KICK;
			p.kick_user_id = kick_user_id;
			Broadcast(reinterpret_cast<char*>(&p), server->GetHandle());

			break;
		}
	}
}

void MultiRoom::StartGame(int id)
{
	if (id != host_id) return;
	if (room_state == ROOM_STATE::PLAY) return;

	int host_index = -1;
	for (int i = 0; i < max_user; i++) { 
		if (room_users[i].GetSession()->GetId() == host_id) {
			host_index = i;
			break;
		}
	}

	if (host_index == -1) return; // host_id가 논리적으로는 존재해야 하지만.. 버그 예외처리

	int ready_user_count = 0;	
	S2C_MULTI_START_PACKET start_p;

	for (auto& r_user : room_users) {
		if (r_user.GetRoomUserState() == ROOM_USER_STATE::EMPTY) continue; // 사용 중이지 않은 인덱스는 건너뜀
		if (r_user.GetSession()->GetId() == host_id) continue; // 방장은 건너뜀

		if (r_user.GetRoomUserState() == ROOM_USER_STATE::WAIT) { // 방에 있는데 레디가 안된 사람이 있으면 시작 불가			
			start_p.size = sizeof(S2C_MULTI_START_PACKET);
			start_p.type = S2C_MULTI_START;
			start_p.is_start = false;

			room_users[host_index].GetSession()->SendPacket(reinterpret_cast<char*>(&start_p), server->GetHandle()); // 시작 불가는 방장에게만 보내면 됨
			return;
		}
		else if (r_user.GetRoomUserState() == ROOM_USER_STATE::READY) ++ready_user_count;
	}

	if (ready_user_count == 0) { // 방장을 제외하고 사람 자체가 없으면 시작 불가, 레디 안한 사람은 위에서 걸러짐
		start_p.size = sizeof(S2C_MULTI_START_PACKET);
		start_p.type = S2C_MULTI_START;
		start_p.is_start = false;

		room_users[host_index].GetSession()->SendPacket(reinterpret_cast<char*>(&start_p), server->GetHandle()); // 시작 불가는 방장에게만 보내면 됨
		// 보내야 할까? 시작 불가 알림창 정도는  클라에게 맏겨도 될 듯 하다. 잘못 와도 시작만 안하면 되니까
		return;
	}
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

	start_p.size = sizeof(S2C_MULTI_START_PACKET);
	start_p.type = S2C_MULTI_START;
	start_p.is_start = true;
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

// 얘는 순차적으로 쌓인 작업을 처리해 보내야할 패킷들을 버퍼에 쌓음
void MultiRoom::BoundPackets(RoomSession& r_session, std::vector<TaskType>& tasks)
{
	for (auto& task : tasks) {
		switch (task.event_type) {
		case EVENT_TYPE::MOVE: {
			// 각 무브별 틱 초기화 추가가 애매하므로, 무브 틱 값 초기화는 Tetris::HandleTetrominoKeyInput에서 처리
			auto& t = std::get<TaskMove>(task.task);
			MakeMovePacket(r_session, static_cast<int>(t.move_type));
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
				if (CheckWinner()) {
					ClearGame();
				}
				return;
			}

			std::vector<char> index_lines = r_session.GetTetris().ClearLine();
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
			r_session.SetRoomUserState(ROOM_USER_STATE::GAMEOVER);
			S2C_GAMEOVER_PACKET send_p;
			send_p.size = sizeof(S2C_GAMEOVER_PACKET);
			send_p.type = S2C_GAMEOVER;
			send_p.id = r_session.GetSession()->GetId();
			r_session.AddToSendBuffer(reinterpret_cast<char*>(&send_p), send_p.size);
			r_session.GetSession()->SendBoundPacket(reinterpret_cast<char*>(&send_p), send_p.size, server->GetHandle());

			if (CheckWinner()) {
				ClearGame();
			}
			return;
		}
		}
	}
}

bool MultiRoom::CheckWinner()
{
	int over_count = 0;
	int player_count = 0;
	for (auto& r_user : room_users) {
		if (r_user.GetRoomUserState() == ROOM_USER_STATE::PLAY) ++player_count;
		if (r_user.GetRoomUserState() == ROOM_USER_STATE::GAMEOVER) ++over_count;
	}

	if ((player_count - over_count) > 1) return false; // 아직 승자가 결정되지 않음
	else {
		int winner_id = -1;
		for (auto& r_user : room_users) {
			if (r_user.GetRoomUserState() == ROOM_USER_STATE::PLAY) winner_id = r_user.GetSession()->GetId();
		}

		S2C_GAMEEND_PACKET gameend_p;
		gameend_p.size = sizeof(S2C_GAMEEND_PACKET);
		gameend_p.type = S2C_GAMEEND;
		gameend_p.winner_id = winner_id;
		Broadcast(reinterpret_cast<char*>(&gameend_p), server->GetHandle());
	
		return true;
	}
}

int MultiRoom::FindNewHost(int delete_id)
{
	for (auto& r_user : room_users) {
		if (r_user.GetRoomUserState() != ROOM_USER_STATE::EMPTY) {
			if (r_user.GetSession()->GetId() == delete_id) continue;
			return r_user.GetSession()->GetId();
		}
	}
	return -1;
}

