#include "MultiRoom.h"

MultiRoom::MultiRoom(IOCPServer* server, Session& session, OpenRoomInitData data)
	: TetrisRoom(server, session, data)
{
	host_id = session.GetDBInfo().id;
}

MultiRoom::MultiRoom(IOCPServer* server, Session& session, LockRoomInitData data)
	: TetrisRoom(server, session, data)
{
	host_id = session.GetDBInfo().id;
}

MultiRoom::~MultiRoom()
{
}

// 게임 시작 전에 처리되는 것들 -> 함수 내에 뮤텍스 넣고 처리
// 게임 시작 후에 처리되는 것들 -> 틱 처리 함수에 뮤텍스 넣고, 틱 처리 관련 내부 함수는 뮤텍스 넣지 않음
void MultiRoom::HandlePacket(char* packet, Session& request_session)
{
	switch (packet[2]) {

	case C2S_DELETE_USER: {
		C2S_DELETE_USER_PACKET* delete_p = reinterpret_cast<C2S_DELETE_USER_PACKET*>(packet);
		DeleteUser(request_session.GetDBInfo().id);
		break;
	}

	case C2S_READY: {
		C2S_READY_PACKET* ready_p = reinterpret_cast<C2S_READY_PACKET*>(packet);
		ReadyUser(request_session.GetDBInfo().id);
		break;
	}

	case C2S_KICK: {
		C2S_KICK_PACKET* kick_p = reinterpret_cast<C2S_KICK_PACKET*>(packet);
		KickUser(request_session.GetDBInfo().id, kick_p->kick_user_id);
		break;
	}

	case C2S_START: {
		//C2S_START_PACKET* recv_p = reinterpret_cast<C2S_START_PACKET*>(packet);
		StartGame(request_session.GetDBInfo().id);
		break;
	}

	case C2S_MOVE: {
		C2S_MOVE_PACKET* recv_p = reinterpret_cast<C2S_MOVE_PACKET*>(packet);
		TaskInfo new_task;
		new_task.id = request_session.GetDBInfo().id;
		new_task.type = static_cast<EVENT_TYPE>(recv_p->move_type);
		GetTasks().AddTask(new_task);
		break;
	}
	}
}

// add는 외부에서 추가되므로 아직 세션이 안전하지 않음. 방에 완전히 들어와야 안전해짐. 락은 외부에서 건다
int MultiRoom::AddUser(Session& new_session, int request_gen, const char* input_password)
{
	if (room_password) {
		if (memcmp(room_password, input_password, MAX_ROOM_PASSWORD) != 0) {
			return ERROR_CODE::ROOM_INVALID_PASSWORD;
		}
	}
	//C2S_ADD_USER_PACKET* recv_p = reinterpret_cast<C2S_ADD_USER_PACKET*>(packet);
	int result = ERROR_CODE::ROOM_FULL;
	int added_slot = -1;
	//std::lock_guard<std::mutex> lock(room_mutex); 외부에서 걸 예정
	if (room_state == ROOM_STATE::WAIT) {
		for (int i = 0; i < room_users.size(); ++i) {
			auto& r_user = room_users[i];
			if (r_user.GetRoomUserState() != ROOM_USER_STATE::EMPTY) continue;

			else {
				std::lock_guard<std::mutex> lock(new_session.GetMutex());
				if (new_session.GetState() == SESS_STATE::NONE) return ERROR_CODE::INVALID_REQUEST; // 반환값이 있어야 해서 일단 억지로 넣은 느낌..
				if (new_session.GetSessionKey().gen != request_gen) return ERROR_CODE::INVALID_REQUEST;

				new_session.StoreState(SESS_STATE::ROOM);
				new_session.SetRoomIndex(room_index);
				r_user.InitRoomSession(new_session);
				++cur_user;
				result = SUCCESS;
				added_slot = i;
				break;
			}
		}
	}

	if (result == SUCCESS) {
		if (!room_password) {
			S2C_ADD_OPEN_ROOM_PACKET open_p;
			open_p.size = sizeof(S2C_ADD_OPEN_ROOM_PACKET);
			open_p.type = S2C_ADD_OPEN_ROOM;
			open_p.gen = room_gen;
			open_p.max_user = max_user;
			memcpy(&open_p.room_name, room_name, MAX_ROOM_NAME);
			room_users[added_slot].GetSession()->SendPacket(reinterpret_cast<char*>(&open_p), server->GetHandle());
		}
		else {
			S2C_ADD_LOCK_ROOM_PACKET lock_p;
			lock_p.size = sizeof(S2C_ADD_LOCK_ROOM_PACKET);
			lock_p.type = S2C_ADD_LOCK_ROOM;
			lock_p.gen = room_gen;
			lock_p.max_user = max_user;
			memcpy(&lock_p.room_name, room_name, MAX_ROOM_NAME);
			memcpy(&lock_p.room_password, room_password, MAX_ROOM_PASSWORD);
			room_users[added_slot].GetSession()->SendPacket(reinterpret_cast<char*>(&lock_p), server->GetHandle());
		}

		// 본인의 입장을 본인 제외 나머지에게(방 생성 시 본인은 방에 추가된다)
		for (int i = 0; i < room_users.size(); ++i) {
			if (i == added_slot) continue;
			auto& r_user = room_users[i];
			S2C_ADD_USER_PACKET add_p;
			add_p.size = sizeof(S2C_ADD_USER_PACKET);
			add_p.type = S2C_ADD_USER;
			add_p.id = new_session.GetDBInfo().id;
			memcpy(&add_p.name, &new_session.GetDBInfo().nickname, MAX_USER_NAME);
			r_user.GetSession()->SendPacket(reinterpret_cast<char*>(&add_p), server->GetHandle());
		}

		// 본인 제외 나머지 유저를 본인에게
		for (int i = 0; i < room_users.size(); ++i)	{
			if (i == added_slot) continue;
			auto& r_user = room_users[i];
			S2C_ADD_USER_PACKET add_p;
			add_p.size = sizeof(S2C_ADD_USER_PACKET);
			add_p.type = S2C_ADD_USER;
			add_p.id = r_user.GetSession()->GetDBInfo().id;
			memcpy(&add_p.name, &r_user.GetSession()->GetDBInfo().nickname, MAX_USER_NAME);
			new_session.SendPacket(reinterpret_cast<char*>(&add_p), server->GetHandle());
		}

		// 새로 입장한 세션에게 방장이 누구인지
		S2C_UPDATE_HOST_PACKET host_p;
		host_p.size = sizeof(S2C_UPDATE_HOST_PACKET);
		host_p.type = S2C_UPDATE_HOST;
		host_p.new_host_id = host_id;
		new_session.SendPacket(reinterpret_cast<char*>(&host_p), server->GetHandle());
		return result;
	}

	else if (room_state == ROOM_STATE::PLAY) result = ERROR_CODE::ROOM_INGAME;

	else if (room_state == ROOM_STATE::WAITING_DELETE) result = ERROR_CODE::ROOM_NOT_FOUND;

	return result;
}

void MultiRoom::DeleteUser(const int id)
{
	std::lock_guard<std::mutex> lock(room_mutex);
	for (auto& r_user : room_users) {
		if (r_user.GetRoomUserState() == ROOM_USER_STATE::EMPTY) continue;
		if (r_user.GetSession()->GetDBInfo().id == id) { // 삭제할 아이디 검색
			S2C_DELETE_USER_PACKET p;
			p.size = sizeof(S2C_DELETE_USER_PACKET);
			p.type = S2C_DELETE_USER;
			p.id = id;

			Broadcast(reinterpret_cast<char*>(&p), server->GetHandle());
			r_user.ClearRoomSession();
			--cur_user;
			if (id == host_id) FindNewHost(); // 여기서 찾기 및 못찾을 경우 삭제까지 같이 함
			break;
		}
	}
}

void MultiRoom::SendCreateRoom(Session& session)
{
	if (!room_password) {
		S2C_ADD_OPEN_ROOM_PACKET open_p;
		open_p.size = sizeof(S2C_ADD_OPEN_ROOM_PACKET);
		open_p.type = S2C_ADD_OPEN_ROOM;
		open_p.gen = room_gen;
		open_p.max_user = max_user;
		memcpy(open_p.room_name, room_name, sizeof(room_name));
		session.SendPacket(reinterpret_cast<char*>(&open_p), server->GetHandle());
	}
	else {
		S2C_ADD_LOCK_ROOM_PACKET lock_p;
		lock_p.size = sizeof(S2C_ADD_LOCK_ROOM_PACKET);
		lock_p.type = S2C_ADD_LOCK_ROOM;
		lock_p.gen = room_gen;
		lock_p.max_user = max_user;
		memcpy(lock_p.room_name, room_name, sizeof(room_name));
		memcpy(lock_p.room_password, room_password, MAX_ROOM_PASSWORD);
		session.SendPacket(reinterpret_cast<char*>(&lock_p), server->GetHandle());
	}
	FindNewHost();
	std::cout << "Room[: " << room_index << "] created by : " << session.GetDBInfo().nickname << "\n";
}

void MultiRoom::ReadyUser(int id)
{
	std::lock_guard<std::mutex> lock(room_mutex);
	if (host_id == id) return;
	bool is_ready = false;
	for (auto& r_user : room_users) {
		if (r_user.GetRoomUserState() == ROOM_USER_STATE::EMPTY) continue;
		if (r_user.GetSession()->GetDBInfo().id == id) { // 레디 상태 
			if (r_user.GetRoomUserState() == ROOM_USER_STATE::READY) {
				r_user.SetRoomUserState(ROOM_USER_STATE::WAIT);
				is_ready = false;
			}
			else if (r_user.GetRoomUserState() == ROOM_USER_STATE::WAIT) {
				r_user.SetRoomUserState(ROOM_USER_STATE::READY);
				is_ready = true;
			}
			S2C_READY_PACKET p;
			p.size = sizeof(S2C_READY_PACKET);
			p.type = S2C_READY;
			p.id = id;
			p.is_ready = is_ready;

			Broadcast(reinterpret_cast<char*>(&p), server->GetHandle());
			break;
		}
	}
}

void MultiRoom::KickUser(int id, int kick_user_id)
{
	std::lock_guard<std::mutex> lock(room_mutex);
	if (id != host_id) return;

	for (auto& r_user : room_users) {
		if (r_user.GetRoomUserState() == ROOM_USER_STATE::EMPTY) continue;
		if (r_user.GetSession()->GetDBInfo().id == kick_user_id) { // 삭제할 아이디 검색

			S2C_DELETE_USER_PACKET p;
			p.size = sizeof(S2C_DELETE_USER_PACKET);
			p.type = S2C_DELETE_USER;
			p.id = kick_user_id;
			Broadcast(reinterpret_cast<char*>(&p), server->GetHandle());

			S2C_INFO_PACKET info_p;
			info_p.size = sizeof(S2C_INFO_PACKET);
			info_p.type = S2C_INFO;
			info_p.info_code = INFO_CODE::KICKED;
			r_user.GetSession()->SendPacket(reinterpret_cast<char*>(&info_p), server->GetHandle());

			r_user.ClearRoomSession();
			--cur_user;
		}
	}
}

void MultiRoom::StartGame(int request_user_id)
{
	std::lock_guard<std::mutex> lock(room_mutex);
	if (request_user_id != host_id) return;
	if (room_state.Load() == ROOM_STATE::PLAY) return;

	int ready_user_count = 0;
	int result = -1;
	for (auto& r_user : room_users) {
		if (r_user.GetRoomUserState() == ROOM_USER_STATE::EMPTY) continue; // 사용 중이지 않은 인덱스는 건너뜀
		if (r_user.GetSession()->GetDBInfo().id == host_id) ++ready_user_count;

		else if (r_user.GetRoomUserState() == ROOM_USER_STATE::READY) ++ready_user_count;
	}

	if (ready_user_count == cur_user) {
		if (cur_user == 1) {
			result = ERROR_CODE::ROOM_NOT_ENOUGH_PLAYERS;
		}
		else {
			result = SUCCESS;
		}
	}
	else result = ERROR_CODE::ROOM_NOT_ALL_READY;

	if (result != SUCCESS) {
		S2C_ERROR_PACKET error_p;
		error_p.size = sizeof(S2C_ERROR_PACKET);
		error_p.type = S2C_ERROR;
		error_p.error_code = result;
		room_users[FindHostIndex(host_id)].GetSession()->SendPacket(reinterpret_cast<char*>(&error_p), server->GetHandle());
		return;
	}

	if(!TryChangeRoomState(ROOM_STATE::WAIT, ROOM_STATE::PLAY)) return; // 잘못된 요청(동시 요청 등)에 대한 방어 코드 -> CAS에 성공해야만 시작
	
	S2C_MULTI_START_PACKET start_p;
	start_p.size = sizeof(S2C_MULTI_START_PACKET);
	start_p.type = S2C_MULTI_START;
	Broadcast(reinterpret_cast<char*>(&start_p), server->GetHandle());
	// 모든 조건 통과->게임 시작
	Add7BagTetrominoList();
	for (auto& r_user : room_users) {
		if (r_user.GetRoomUserState() == ROOM_USER_STATE::EMPTY) continue;
		r_user.SetRoomUserState(ROOM_USER_STATE::PLAY);
		r_user.GetTetris().InitNewTetromino(tetromino_spawn_list[r_user.GetTetrominoIndex()], spawn_pos);
	}

	// 테트리스 게임 중에 들어오는 패킷은 또 따로 분리하고 싶기는 한데..
	InitGame();

	for (auto& r_user : room_users) {
		if (r_user.GetRoomUserState() == ROOM_USER_STATE::EMPTY) continue;
		S2C_SPAWN_PACKET spawn_p;
		spawn_p.size = sizeof(S2C_SPAWN_PACKET);
		spawn_p.type = S2C_SPAWN;
		spawn_p.id = r_user.GetSession()->GetDBInfo().id;
		spawn_p.tetromino_type = tetromino_spawn_list[r_user.GetTetrominoIndex()];
		spawn_p.next_tetromino_type = tetromino_spawn_list[r_user.GetTetrominoIndex() + 1];
		spawn_p.spawn_x = spawn_pos.x;
		spawn_p.spawn_y = spawn_pos.y;
		Broadcast(reinterpret_cast<char*>(&spawn_p), server->GetHandle());
	}
}

void MultiRoom::ProcessPlayTasks()
{
	std::lock_guard<std::mutex> lock(room_mutex); // 게임 중간에 Disconnect 작업이 일어나 DeleteUser가 호출될 수 있음. 틱 처리 이전에 해결하는 것이 좋아보인다.
	tasks.SwapTask();
	while (!tasks.task_queue.IsEmpty()) {
		TaskInfo task = tasks.GetTask();
		for (auto& r_user : room_users) {
			if (r_user.GetRoomUserState() == ROOM_USER_STATE::EMPTY) continue;
			if (r_user.GetSession()->GetDBInfo().id == task.id) {
				// 각 작업들을 각 세션에 분배
				r_user.GetTetris().GetInputTasks().emplace_back(task.type);

				//r_user.GetTetris().DebugPrintBoard();
				break;
			}
		}
	}
	UpdatePrevUsersState();

	for (auto& r_user : room_users) {
		if (r_user.GetRoomUserState() == ROOM_USER_STATE::EMPTY) continue;
		r_user.GetTetris().TickProcess();
	}

	CalcAttackLine();
	AddGarbageLines();
	// 승자 나왔으면 종료 아니면 계속 진행해야되니 스폰 체크
	bool game_end = false;
	game_end = FindWinner();
	if (!game_end) AddSpawnTask();
	BoundPackets();

	ResetUsersTickData();
	BroadcastTickDataForUsers();
	if (game_end) {
		RequestUpdateMatchResult();
		ClearGame();
	}
}

bool MultiRoom::FindWinner()
{
	int over_count = 0;
	int player_count = 0;
	for (auto& r_user : room_users) {
		if (r_user.GetRoomUserState() == ROOM_USER_STATE::PLAY) ++player_count;
		//if (r_user.GetRoomUserState() == ROOM_USER_STATE::GAMEOVER) ++over_count;
	}

	// int winner_id = -1;
	if (player_count == 1) {
		for (auto& r_user : room_users) {
			if (r_user.GetRoomUserState() == ROOM_USER_STATE::PLAY) {
				winner_id = r_user.GetSession()->GetDBInfo().id;
				break;
			}
		}
	}

	else if (player_count == 0) { // 동시에 게임오버된 상태 -> 결국 승자는 정해줘야함.
		for (auto& r_user : room_users) {
			if ((r_user.GetRoomUserState() == ROOM_USER_STATE::GAMEOVER) || (r_user.GetPrevRoomUserState() == ROOM_USER_STATE::PLAY)) {
				winner_id = r_user.GetSession()->GetDBInfo().id; // 컨테이너 앞쪽에 있는 사람이 승자
				break;
			}
		}
	}

	else return false;

	for (auto& r_user : room_users) {
		if (r_user.GetRoomUserState() != ROOM_USER_STATE::EMPTY) {
			TaskType t_type;
			t_type.event_type = EVENT_TYPE::GAMEEND;
			t_type.task = TaskGameEnd{ winner_id };
			r_user.GetTetris().GetSendTasks().emplace_back(t_type);
		}
	}

	return true;
}

void MultiRoom::CalcAttackLine()
{	
	// 서로 클리어한 라인을 통해 남에게 패널티를 부여할 라인을 모두 계산한 뒤, 한 번에 적용
	for (int i = 0; i < room_users.size(); ++i) {
		if (room_users[i].GetRoomUserState() == ROOM_USER_STATE::PLAY) {
			int add_line_num = GetGarbageLinesFromAttack(room_users[i].GetTetris().GetClearedLines());
			if (add_line_num > 0) {
				for (int j = 0; j < room_users.size(); ++j) {
					if ((room_users[j].GetRoomUserState() == ROOM_USER_STATE::PLAY) && (i != j)){
						(room_users[j].GetTetris().AddPendingGarbageLines(add_line_num));
					}
				}
			}
		}
	}

	// 로직은 맞지만 함수 이름상 여기서 이 동작을 추가하는건 안어울리는 것 같기도, 이름과 동작은 명확히 일치해야 할 듯
	//for (auto& r_user : room_users) {
	//	if (r_user.GetRoomUserState() == ROOM_USER_STATE::PLAY) {
	//		r_user.GetTetris().AddGarbageLines();
	//	}
	//}
}

int MultiRoom::GetGarbageLinesFromAttack(int cleared_line_num)
{
	int garbage_lines = 0;
	// 지워진 라인에 따라 증가되는 라인 수가 다름
	switch (cleared_line_num) {
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

void MultiRoom::UpdatePrevUsersState()
{
	if (room_state != ROOM_STATE::PLAY) return;
	for (auto& r_user : room_users) {
		if (r_user.GetRoomUserState() != ROOM_USER_STATE::EMPTY) {
			r_user.SetPrevRoomUserState(r_user.GetRoomUserState());
		}
	}
}

void MultiRoom::RequestUpdateMatchResult()
{
	for (auto& r_user : room_users) {
		if (r_user.GetRoomUserState() != ROOM_USER_STATE::EMPTY) {
			bool is_winner = false;
			if (winner_id == r_user.GetSession()->GetDBInfo().id) {
				is_winner = true;
			}
			Database& db = server->GetDB();
			SessionKey key;
			key.gen = r_user.GetSession()->GetSessionKey().gen;
			key.index = r_user.GetSession()->GetSessionKey().index;
			int user_id = r_user.GetSession()->GetDBInfo().id;
			auto task_update_match_result = [key, user_id, is_winner, &db] {
				db.ExecuteUpdateMatchResult(key, user_id, is_winner);
				};
			db.Enqueue(task_update_match_result);
		}
	}
}

void MultiRoom::FindNewHost()
{
	bool find_host = false;
	
	if (cur_user != 0) {
		for (auto& r_user : room_users) {
			if (r_user.GetRoomUserState() == ROOM_USER_STATE::EMPTY) continue; 
			host_id = r_user.GetSession()->GetDBInfo().id;
			if (room_state == ROOM_STATE::WAIT) r_user.SetRoomUserState(ROOM_USER_STATE::WAIT); // 게임 중이 아닐 때, 호스트가 나갔을 때 레디 상태인 사람이 호스트가 되면, 레디 상태를 풀어줘야함
			find_host = true;
			break;
		}
	}

	if (find_host) {
		S2C_UPDATE_HOST_PACKET host_p;
		host_p.size = sizeof(S2C_UPDATE_HOST_PACKET);
		host_p.type = S2C_UPDATE_HOST;
		host_p.new_host_id = host_id;
		Broadcast(reinterpret_cast<char*>(&host_p), server->GetHandle());
	}

	else {
		room_state.Store(ROOM_STATE::WAITING_DELETE);
		ExOverlapped* delete_over = new ExOverlapped;
		delete_over->op_type = OP_TYPE::DELETE_ROOM;
		PostQueuedCompletionStatus(server->GetHandle(), 1, room_index, reinterpret_cast<WSAOVERLAPPED*>(delete_over));
	}
}

int MultiRoom::FindHostIndex(int host_id)
{
	for (int i = 0; i < room_users.size(); ++i) {
		if (room_users[i].GetSession()->GetDBInfo().id == host_id) return i;
	}
	return -1;
}

