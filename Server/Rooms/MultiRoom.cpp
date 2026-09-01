#include "MultiRoom.h"

MultiRoom::MultiRoom(IOCPServer* server, OpenRoomInitData data)
	: TetrisRoom(server, data)
{
}

MultiRoom::MultiRoom(IOCPServer* server, LockRoomInitData data)
	: TetrisRoom(server, data)
{
}

MultiRoom::~MultiRoom()
{
}

bool MultiRoom::AddHostSession(const SP<Session>& session)
{
	if (!InitHostSession(session)) return false;
	host_id = session->GetDBInfo().id;
	return true;
}

void MultiRoom::HandlePacket(char* packet, const SP<Session>& request_session)
{
	if (!request_session) return;
	switch (reinterpret_cast<PacketHeader*>(packet)->type) {
	case C2S_READY: {
		RoomTaskInfo task;
		task.type = ROOM_TASK_TYPE::READY;
		task.session = request_session;
		AddRoomTask(std::move(task));
		break;
	}
	case C2S_KICK: {
		C2S_KICK_PACKET* kick_p = reinterpret_cast<C2S_KICK_PACKET*>(packet);
		RoomTaskInfo task;
		task.type = ROOM_TASK_TYPE::KICK;
		task.session = request_session;
		task.target_id = kick_p->kick_user_id;
		AddRoomTask(std::move(task));
		break;
	}
	default:
		TetrisRoom::HandlePacket(packet, request_session);
		break;
	}
}

bool MultiRoom::AddUserTask(const SP<Session>& new_session, int matching_max_user)
{
	RoomTaskInfo task;
	task.type = ROOM_TASK_TYPE::JOIN;
	task.session = new_session;
	task.matching_max_user = matching_max_user;
	return AddRoomTask(std::move(task));
}

int MultiRoom::AddUser(const SP<Session>& new_session)
{
	if (!new_session) return ERROR_CODE::INVALID_REQUEST;
	if (!IsRoomSession(new_session)) return ERROR_CODE::INVALID_REQUEST;
	auto room_users = GetRoomUsers();
	//C2S_ADD_USER_PACKET* recv_p = reinterpret_cast<C2S_ADD_USER_PACKET*>(packet);
	int result = ERROR_CODE::ROOM_FULL;
	int added_slot = -1;
	if (room_state == ROOM_STATE::WAIT) {
		for (int i = 0; i < room_users.size(); ++i) {
			auto& r_user = room_users[i];
			if (r_user.GetSession()) continue;

			else {
				if (!r_user.InitRoomSession(new_session, room_index)) return ERROR_CODE::INVALID_REQUEST;
				++cur_user;
				result = SUCCESS;
				added_slot = i;
				break;
			}
		}
	}

	if (result == SUCCESS) {
		if (room_password.empty()) {
			S2C_ADD_OPEN_ROOM_PACKET open_p;
			open_p.header.size = static_cast<std::uint16_t>(sizeof(open_p));
			open_p.header.type = S2C_ADD_OPEN_ROOM;
			open_p.gen = room_gen;
			open_p.max_user = max_user;
			server->StringToCharBuf(room_name, open_p.room_name, sizeof(open_p.room_name));
			new_session->SendPacket(reinterpret_cast<char*>(&open_p), server->GetHandle());
		}
		else {
			S2C_ADD_LOCK_ROOM_PACKET lock_p;
			lock_p.header.size = static_cast<std::uint16_t>(sizeof(lock_p));
			lock_p.header.type = S2C_ADD_LOCK_ROOM;
			lock_p.gen = room_gen;
			lock_p.max_user = max_user;
			server->StringToCharBuf(room_name, lock_p.room_name, sizeof(lock_p.room_name));
			server->StringToCharBuf(room_password, lock_p.room_password, sizeof(lock_p.room_password));
			new_session->SendPacket(reinterpret_cast<char*>(&lock_p), server->GetHandle());
		}

		// 본인의 입장을 본인 제외 나머지에게(방 생성 시 본인은 방에 추가된다)
		for (int i = 0; i < room_users.size(); ++i) {
			if (i == added_slot) continue;
			auto& r_user = room_users[i];
			auto session = r_user.GetSession();
			if (!session) continue;
			S2C_ADD_USER_PACKET add_p;
			add_p.header.size = static_cast<std::uint16_t>(sizeof(add_p));
			add_p.header.type = S2C_ADD_USER;
			add_p.id = new_session->GetDBInfo().id;
			server->StringToCharBuf(new_session->GetDBInfo().nickname, add_p.name, sizeof(add_p.name));
			session->SendPacket(reinterpret_cast<char*>(&add_p), server->GetHandle());
		}

		// 본인 제외 나머지 유저를 본인에게
		for (int i = 0; i < room_users.size(); ++i)	{
			if (i == added_slot) continue;
			auto& r_user = room_users[i];
			auto session = r_user.GetSession();
			if (!session) continue;
			S2C_ADD_USER_PACKET add_p;
			add_p.header.size = static_cast<std::uint16_t>(sizeof(add_p));
			add_p.header.type = S2C_ADD_USER;
			add_p.id = session->GetDBInfo().id;
			server->StringToCharBuf(session->GetDBInfo().nickname, add_p.name, sizeof(add_p.name));
			new_session->SendPacket(reinterpret_cast<char*>(&add_p), server->GetHandle());
		}

		// 새로 입장한 세션에게 방장이 누구인지
		S2C_UPDATE_HOST_PACKET host_p;
		host_p.header.size = static_cast<std::uint16_t>(sizeof(host_p));
		host_p.header.type = S2C_UPDATE_HOST;
		host_p.new_host_id = host_id;
		new_session->SendPacket(reinterpret_cast<char*>(&host_p), server->GetHandle());
		return result;
	}

	else if (room_state == ROOM_STATE::PLAY) result = ERROR_CODE::ROOM_INGAME;

	else if (room_state == ROOM_STATE::WAITING_DELETE) result = ERROR_CODE::ROOM_NOT_FOUND;

	return result;
}

void MultiRoom::DeleteUser(const int id)
{
	auto room_users = GetRoomUsers();
	for (auto& r_user : room_users) {
		auto session = r_user.GetSession();
		if (!session) continue;
		if (session->GetDBInfo().id == id) { // 삭제할 아이디 검색
			if (cur_user.load() == 1) BeginRoomDelete();
			S2C_DELETE_USER_PACKET p;
			p.header.size = static_cast<std::uint16_t>(sizeof(p));
			p.header.type = S2C_DELETE_USER;
			p.id = id;

			Broadcast(reinterpret_cast<char*>(&p), server->GetHandle());
			r_user.ClearRoomSession();
			--cur_user;
			if (id == host_id) FindNewHost(); // 여기서 찾기 및 못찾을 경우 삭제까지 같이 함
			break;
		}
	}
}

void MultiRoom::SendCreateRoom(const SP<Session>& session)
{
	if (!session) return;
	if (room_password.empty()) {
		S2C_ADD_OPEN_ROOM_PACKET open_p;
		open_p.header.size = static_cast<std::uint16_t>(sizeof(open_p));
		open_p.header.type = S2C_ADD_OPEN_ROOM;
		open_p.gen = room_gen;
		open_p.max_user = max_user;
		server->StringToCharBuf(room_name, open_p.room_name, sizeof(open_p.room_name));
		session->SendPacket(reinterpret_cast<char*>(&open_p), server->GetHandle());
	}
	else {
		S2C_ADD_LOCK_ROOM_PACKET lock_p;
		lock_p.header.size = static_cast<std::uint16_t>(sizeof(lock_p));
		lock_p.header.type = S2C_ADD_LOCK_ROOM;
		lock_p.gen = room_gen;
		lock_p.max_user = max_user;
		server->StringToCharBuf(room_name, lock_p.room_name, sizeof(lock_p.room_name));
		server->StringToCharBuf(room_password, lock_p.room_password, sizeof(lock_p.room_password));
		session->SendPacket(reinterpret_cast<char*>(&lock_p), server->GetHandle());
	}
	FindNewHost();
	std::cout << "방 생성 - 방 이름: " << room_name << ", 플레이어: " << session->GetDBInfo().nickname << std::endl;
}

void MultiRoom::ReadyUser(int id)
{
	if (host_id == id) return;
	auto room_users = GetRoomUsers();
	bool is_ready = false;
	for (auto& r_user : room_users) {
		auto session = r_user.GetSession();
		if (!session) continue;
		if (session->GetDBInfo().id == id) { // 레디 상태 
			if (r_user.GetRoomUserState() == ROOM_USER_STATE::READY) {
				r_user.SetRoomUserState(ROOM_USER_STATE::WAIT);
				is_ready = false;
			}
			else if (r_user.GetRoomUserState() == ROOM_USER_STATE::WAIT) {
				r_user.SetRoomUserState(ROOM_USER_STATE::READY);
				is_ready = true;
			}
			S2C_READY_PACKET p;
			p.header.size = static_cast<std::uint16_t>(sizeof(p));
			p.header.type = S2C_READY;
			p.id = id;
			p.is_ready = is_ready;

			Broadcast(reinterpret_cast<char*>(&p), server->GetHandle());
			break;
		}
	}
}

void MultiRoom::KickUser(int id, int kick_user_id)
{
	if (id != host_id) return;
	if (kick_user_id == host_id) return;
	auto room_users = GetRoomUsers();

	for (auto& r_user : room_users) {
		auto session = r_user.GetSession();
		if (!session) continue;
		if (session->GetDBInfo().id == kick_user_id) { // 삭제할 아이디 검색

			S2C_DELETE_USER_PACKET p;
			p.header.size = static_cast<std::uint16_t>(sizeof(p));
			p.header.type = S2C_DELETE_USER;
			p.id = kick_user_id;
			Broadcast(reinterpret_cast<char*>(&p), server->GetHandle());

			S2C_INFO_PACKET info_p;
			info_p.header.size = static_cast<std::uint16_t>(sizeof(info_p));
			info_p.header.type = S2C_INFO;
			info_p.info_code = INFO_CODE::KICKED;
			session->SendPacket(reinterpret_cast<char*>(&info_p), server->GetHandle());

			r_user.ClearRoomSession();
			--cur_user;
		}
	}
}

void MultiRoom::StartGame(int request_user_id)
{
	if (request_user_id != host_id) return;
	if (room_state.load() == ROOM_STATE::PLAY) return;
	auto room_users = GetRoomUsers();

	int ready_user_count = 0;
	int result = -1;
	for (auto& r_user : room_users) {
		auto session = r_user.GetSession();
		if (!session) continue; // 사용 중이지 않은 인덱스는 건너뜀
		if (session->GetDBInfo().id == host_id) ++ready_user_count;

		else if (r_user.GetRoomUserState() == ROOM_USER_STATE::READY) ++ready_user_count;
	}

	int current_user = GetCurrentUser();
	if (ready_user_count == current_user) {
		if (current_user == 1) {
			result = ERROR_CODE::ROOM_NOT_ENOUGH_PLAYERS;
		}
		else {
			result = SUCCESS;
		}
	}
	else result = ERROR_CODE::ROOM_NOT_ALL_READY;

	if (result != SUCCESS) {
		S2C_ERROR_PACKET error_p;
		error_p.header.size = static_cast<std::uint16_t>(sizeof(error_p));
		error_p.header.type = S2C_ERROR;
		error_p.error_code = result;
		int host_index = FindHostIndex(host_id);
		if (host_index >= 0) {
			auto host_session = room_users[host_index].GetSession();
			if (host_session) host_session->SendPacket(reinterpret_cast<char*>(&error_p), server->GetHandle());
		}
		return;
	}

	if(!TryChangeRoomState(ROOM_STATE::WAIT, ROOM_STATE::PLAY)) return; // 잘못된 요청(동시 요청 등)에 대한 방어 코드 -> CAS에 성공해야만 시작
	play_generation.fetch_add(1);
	
	S2C_MULTI_START_PACKET start_p;
	start_p.header.size = static_cast<std::uint16_t>(sizeof(start_p));
	start_p.header.type = S2C_MULTI_START;
	Broadcast(reinterpret_cast<char*>(&start_p), server->GetHandle());
	// 모든 조건 통과->게임 시작
	Add7BagTetrominoList();
	for (auto& r_user : room_users) {
		auto session = r_user.GetSession();
		if (!session) continue;
		r_user.SetRoomUserState(ROOM_USER_STATE::PLAY);
		r_user.GetTetris().InitNewTetromino(tetromino_spawn_list[r_user.GetTetrominoIndex()], spawn_pos);
	}

	// 테트리스 게임 중에 들어오는 패킷은 또 따로 분리하고 싶기는 한데..
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

void MultiRoom::ProcessSpecificRoomTask(const RoomTaskInfo& task)
{
	switch (task.type) {
	case ROOM_TASK_TYPE::JOIN: {
		const int result = AddUser(task.session);
		if (result == SUCCESS) break;
		if (!IsRoomSession(task.session)) break;
		task.session->SetRoomSnapShot(MODE_STATE::LOBBY, -1);
		if (task.session->GetLifeState() != LIFE_STATE::ACTIVE) break;
		if (task.matching_max_user >= 0) server->FindMatch(task.session, task.matching_max_user);
		else server->SendError(task.session, result);
		break;
	}
	case ROOM_TASK_TYPE::READY:
		if (IsRoomSession(task.session)) ReadyUser(task.session->GetDBInfo().id);
		break;
	case ROOM_TASK_TYPE::KICK:
		if (IsRoomSession(task.session)) KickUser(task.session->GetDBInfo().id, task.target_id);
		break;
	case ROOM_TASK_TYPE::START:
		if (IsRoomSession(task.session)) StartGame(task.session->GetDBInfo().id);
		break;
	default:
		break;
	}
}

void MultiRoom::ProcessGameTick()
{
	UpdatePrevUsersState();

	auto room_users = GetRoomUsers();
	for (auto& r_user : room_users) {
		auto session = r_user.GetSession();
		if (!session) continue;
		r_user.GetTetris().TickProcess();
	}

	CalcAttackLine();
	AddGarbageLines();
	// 승자 나왔으면 종료 아니면 계속 진행해야되니 스폰 체크
	bool game_end = false;
	game_end = FindWinner();
	if (!game_end) AddSpawnTask();
	int failed_user_id = BoundPackets();
	if (failed_user_id != -1) {
		DeleteUser(failed_user_id);
		TryPostRoomDelete();
		return;
	}

	ResetUsersTickData();
	BroadcastTickDataForUsers();
	if (game_end) {
		RequestUpdateMatchResult();
		ClearGame();
	}
}

bool MultiRoom::FindWinner()
{
	auto room_users = GetRoomUsers();
	int over_count = 0;
	int player_count = 0;
	for (auto& r_user : room_users) {
		if (r_user.GetRoomUserState() != ROOM_USER_STATE::PLAY) continue;
		auto session = r_user.GetSession();
		if (!session) continue;
		++player_count;
		//if (r_user.GetRoomUserState() == ROOM_USER_STATE::GAMEOVER) ++over_count;
	}

	// int winner_id = -1;
	int new_winner_id = -1;
	if (player_count == 1) {
		for (auto& r_user : room_users) {
			if (r_user.GetRoomUserState() == ROOM_USER_STATE::PLAY) {
				auto session = r_user.GetSession();
				if (!session) continue;
				new_winner_id = session->GetDBInfo().id;
				break;
			}
		}
	}

	else if (player_count == 0) { // 동시에 게임오버된 상태 -> 결국 승자는 정해줘야함.
		for (auto& r_user : room_users) {
			if ((r_user.GetRoomUserState() == ROOM_USER_STATE::GAMEOVER) || (r_user.GetPrevRoomUserState() == ROOM_USER_STATE::PLAY)) {
				auto session = r_user.GetSession();
				if (!session) continue;
				new_winner_id = session->GetDBInfo().id; // 컨테이너 앞쪽에 있는 사람이 승자
				break;
			}
		}
	}

	else return false;
	if (new_winner_id == -1) return false;
	winner_id = new_winner_id;

	for (auto& r_user : room_users) {
		auto session = r_user.GetSession();
		if (!session) continue;
		TaskType t_type;
		t_type.event_type = EVENT_TYPE::GAMEEND;
		t_type.task = TaskGameEnd{ winner_id };
		r_user.GetTetris().GetSendTasks().emplace_back(t_type);
	}

	return true;
}

void MultiRoom::CalcAttackLine()
{	
	// 서로 클리어한 라인을 통해 남에게 패널티를 부여할 라인을 모두 계산한 뒤, 한 번에 적용
	auto room_users = GetRoomUsers();
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
	auto room_users = GetRoomUsers();
	for (auto& r_user : room_users) {
		auto session = r_user.GetSession();
		if (!session) continue;
		r_user.SetPrevRoomUserState(r_user.GetRoomUserState());
	}
}

void MultiRoom::RequestUpdateMatchResult()
{
	SessionKey winner_key{};
	winner_key.id = winner_id;
	SessionKey players[MAX_MATCH_RESULT_PLAYERS]{};
	SP<Session> sessions[MAX_MATCH_RESULT_PLAYERS]{};
	uint8_t player_count = 0;

	auto room_users = GetRoomUsers();
	for (auto& r_user : room_users) {
		auto session = r_user.GetSession();
		if (!session) continue;
		players[player_count] = session->GetSessionKey();
		sessions[player_count] = session;
		if (players[player_count].id == winner_id) winner_key = players[player_count];
		++player_count;
	}

	server->EnqueueDBTask(std::make_unique<DBUpdateMatchResultTask>(winner_key, players, player_count), sessions);
}

void MultiRoom::FindNewHost()
{
	bool find_host = false;
	auto room_users = GetRoomUsers();
	
	if (GetCurrentUser() != 0) {
		for (auto& r_user : room_users) {
			auto session = r_user.GetSession();
			if (!session) continue;
			host_id = session->GetDBInfo().id;
			if (room_state == ROOM_STATE::WAIT) r_user.SetRoomUserState(ROOM_USER_STATE::WAIT); // 게임 중이 아닐 때, 호스트가 나갔을 때 레디 상태인 사람이 호스트가 되면, 레디 상태를 풀어줘야함
			find_host = true;
			break;
		}
	}

	if (find_host) {
		S2C_UPDATE_HOST_PACKET host_p;
		host_p.header.size = static_cast<std::uint16_t>(sizeof(host_p));
		host_p.header.type = S2C_UPDATE_HOST;
		host_p.new_host_id = host_id;
		Broadcast(reinterpret_cast<char*>(&host_p), server->GetHandle());
	}

	else {
		BeginRoomDelete();
	}
}

int MultiRoom::FindHostIndex(int host_id)
{
	auto room_users = GetRoomUsers();
	for (int i = 0; i < room_users.size(); ++i) {
		auto session = room_users[i].GetSession();
		if (!session) continue;
		if (session->GetDBInfo().id == host_id) return i;
	}
	return -1;
}

