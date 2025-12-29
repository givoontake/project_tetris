#include <random>
#include "TetrisRoom.h"
TetrisRoom::TetrisRoom(IOCPServer* server) : server(server), room_handler(this, server)
{
	
}

TetrisRoom::~TetrisRoom()
{

}

void TetrisRoom::InitRoom(char* packet, Session* session) // 네트워크 절약을 위해 비밀번호 포함 여부를 다르게 하여 패킷을 두 개로 구분, 같은 역할이므로 하나의 함수로 받아 케이스로 처리
{
	switch (packet[2]) {
	case C2S_ADD_OPEN_ROOM: {
		C2S_ADD_OPEN_ROOM_PACKET* p = reinterpret_cast<C2S_ADD_OPEN_ROOM_PACKET*>(packet);
		if (!(p->max_user == 1 || p->max_user == 2 || p->max_user == 5)) break;
		host_id = session->GetId();
		max_user = p->max_user;
		// resize는 만약 기존 벡터 메모리가 부족할 경우 새 메모리를 할당하고 기존 메모리 내용을 복사하는데, 여기서 아토믹 복사 불가 문제가 발생할 수 있음
		room_users.reserve(p->max_user); // 미리 메모리를 할당하고 객체를 채우면 문제 x
		for (int i = 0; i < p->max_user; ++i){
			room_users.emplace_back(); 
		}
		memcpy(room_name, p->room_name, sizeof(room_name));
		is_password = false;
		S2C_ADD_OPEN_ROOM_PACKET send_p;
		send_p.size = sizeof(S2C_ADD_OPEN_ROOM_PACKET);
		send_p.type = S2C_ADD_OPEN_ROOM;
		send_p.id = session->GetId();
		send_p.max_user = max_user;
		memcpy(send_p.room_name, p->room_name, sizeof(room_name));
		session->SendPacket(reinterpret_cast<char*>(&send_p), server->GetHandle());
		AddUser(session);
		break;
	}

	case C2S_ADD_LOCK_ROOM:{
		C2S_ADD_LOCK_ROOM_PACKET* p = reinterpret_cast<C2S_ADD_LOCK_ROOM_PACKET*>(packet);
		if (!(p->max_user == 1 || p->max_user == 2 || p->max_user == 5)) break;
		host_id = session->GetId();
		max_user = p->max_user;
		room_users.reserve(p->max_user);
		for (int i = 0; i < p->max_user; ++i) {
			room_users.emplace_back();
		}
		memcpy(room_name, p->room_name, sizeof(room_name));
		is_password = true;
		memcpy(room_password, p->room_password, sizeof(room_password));
		S2C_ADD_LOCK_ROOM_PACKET send_p;
		send_p.size = sizeof(S2C_ADD_LOCK_ROOM_PACKET);
		send_p.type = S2C_ADD_LOCK_ROOM;
		send_p.id = session->GetId();
		send_p.max_user = max_user;
		memcpy(send_p.room_name, p->room_name, sizeof(room_name));
		memcpy(send_p.room_password, p->room_password, sizeof(room_password));
		session->SendPacket(reinterpret_cast<char*>(&send_p), server->GetHandle());
		AddUser(session);
		break;
	}
	}
	std::cout << "Session id: " << session->GetId() << " Create room. room id(index): " << room_id;
}

void TetrisRoom::AddUser(Session* new_session)
{
	//C2S_ADD_USER_PACKET* recv_p = reinterpret_cast<C2S_ADD_USER_PACKET*>(packet);

	bool b_send = false;
	for(auto& r_user : room_users){
		if (r_user.GetRoomUserState() != ROOM_USER_STATE::EMPTY) continue;
			
		else {
			new_session->SetState(ROOM);
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

void TetrisRoom::DeleteUser(const int id)
{
	//std::cout << "delete user id: " << id << std::endl;
	for (auto& r_user : room_users){
		if (r_user.GetRoomUserState() == ROOM_USER_STATE::EMPTY) continue;
		if (r_user.GetSession()->GetId() == id) { // 삭제할 아이디 검색
			//std::cout << "delete user id: " << id << std::endl;
			//room_mutex.lock();
			r_user.GetSession()->SetState(LOBBY);

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
				ClearRoom();
				std::cout << "Room index " << room_id << " now empty" << std::endl;
			}
			r_user.ClearSession(); // 해당 아이디 세션 정리

			break;
			//room_mutex.unlock();
		}
	}
}

void TetrisRoom::ReadyUser(int id)
{
	if (host_id == id) return;

	for (auto& r_user : room_users){
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


void TetrisRoom::KickUser(int id, int kick_user_id)
{
	if (id != host_id) return;

	for (auto& r_user : room_users) {
		if (r_user.GetSession()->GetId() == kick_user_id) { // 삭제할 아이디 검색
			r_user.ClearSession(); // 해당 아이디 세션 정리
			r_user.SetRoomUserState(ROOM_USER_STATE::EMPTY);
			r_user.GetSession()->SetState(LOBBY);

			S2C_KICK_PACKET p;
			p.size = sizeof(S2C_KICK_PACKET);
			p.type = S2C_KICK;
			p.kick_user_id = kick_user_id;
			Broadcast(reinterpret_cast<char*>(&p), server->GetHandle());

			break;
		}
	}
}

void TetrisRoom::StartGame(int id)
{
	if (id != host_id) return;
	if (room_state == PLAY) return;

	int host_index = -1;
	for (int i = 0; i < max_user; i++){
		if (room_users[i].GetSession()->GetId() == host_id) {
			host_index = i;
			break;
		}
	}

	if(host_index == -1) return; // host_id가 논리적으로는 존재해야 하지만.. 버그 예외처리

	int ready_user_count = 0;
	S2C_START_PACKET start_p;

	for(auto& r_user : room_users){
		if (r_user.GetRoomUserState() == ROOM_USER_STATE::EMPTY) continue; // 사용 중이지 않은 인덱스는 건너뜀
		if (r_user.GetSession()->GetId() == host_id) continue; // 방장은 건너뜀

		if (r_user.GetRoomUserState() == ROOM_USER_STATE::WAIT) { // 방에 있는데 레디가 안된 사람이 있으면 시작 불가			
			start_p.size = sizeof(S2C_START_PACKET);
			start_p.type = S2C_START;
			start_p.is_start = false;

			room_users[host_index].GetSession()->SendPacket(reinterpret_cast<char*>(&start_p), server->GetHandle()); // 시작 불가는 방장에게만 보내면 됨
			return;
		}
		else ++ready_user_count;
	}

	if (max_user != 1 && ready_user_count == 0) { // 방장만 존재하면 당연히 시작 불가
		start_p.size = sizeof(S2C_START_PACKET);
		start_p.type = S2C_START;
		start_p.is_start = false;
		start_p.score = 0;

		room_users[host_index].GetSession()->SendPacket(reinterpret_cast<char*>(&start_p), server->GetHandle()); // 시작 불가는 방장에게만 보내면 됨
		// 보내야 할까? 시작 불가 알림창 정도는  클라에게 맏겨도 될 듯 하다. 잘못 와도 시작만 안하면 되니까
		return;
	}
	SetRoomState(PLAY);

	// 모든 조건 통과->게임 시작
	Add7BagTetrominoList();
	for (auto& r_user : room_users){
		if (r_user.GetRoomUserState() == ROOM_USER_STATE::EMPTY) continue;
		r_user.SetRoomUserState(ROOM_USER_STATE::PLAY);
		r_user.GetTetris().InitNewTetromino(tetromino_spawn_list[r_user.GetTetrominoIndex()], spawn_pos);
	}

	// 테트리스 게임 중에 들어오는 패킷은 또 따로 분리하고 싶기는 한데..
	InitGame();

	start_p.size = sizeof(S2C_START_PACKET);
	start_p.type = S2C_START;
	start_p.is_start = true;
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

void TetrisRoom::BuildBroadcastData(RoomSession& r_session, std::vector<TaskType>& tasks)
{
	for(auto& task : tasks){
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

			// 줄 추가시 게임 오버가 될 수도 있지만 블록 고정시에도 게임 오버가 될 수 있다. 이것 역시 fix와 동반되는 과정이다.
			if (r_session.GetTetris().CheckGameover()) {
				r_session.SetRoomUserState(ROOM_USER_STATE::WAIT);
				S2C_GAMEOVER_PACKET send_p; // 싱글은 게임오버 = 게임 끝
				send_p.size = sizeof(S2C_GAMEOVER_PACKET);
				send_p.type = S2C_GAMEOVER;
				send_p.id = r_session.GetSession()->GetId();
				r_session.AddToSendBuffer(reinterpret_cast<char*>(&send_p), send_p.size);
				r_session.GetSession()->SendBoundPacket(reinterpret_cast<char*>(&send_p), send_p.size, server->GetHandle());
				if (max_user == 1) {
					ClearGame();
				}
				else {
					if (CheckWinner()) {
						ClearGame();
					}
				}
				return;
			}

			S2C_FIX_PACKET fix_p;
			fix_p.size = sizeof(S2C_FIX_PACKET);
			fix_p.type = S2C_FIX;
			fix_p.id = r_session.GetSession()->GetId();
			fix_p.fixed_x = r_session.GetTetris().GetCurrentTetromino().moved_pos.x;
			fix_p.fixed_y = r_session.GetTetris().GetCurrentTetromino().moved_pos.y;
			r_session.AddToSendBuffer(reinterpret_cast<char*>(&fix_p), fix_p.size);

			r_session.GetTetris().CheckGameover();

			std::vector<char> index_lines = r_session.GetTetris().ClearLine();
			if (!index_lines.empty()) {
				r_session.SetScore(r_session.GetScore() + (CLEAR_LINE_SCORE * index_lines.size() * index_lines.size()));
				for (int i = 0; i < index_lines.size(); i++) {
					S2C_CLEARLINE_PACKET clear_line_p;
					clear_line_p.size = sizeof(S2C_CLEARLINE_PACKET);
					clear_line_p.type = S2C_CLEARLINE;
					clear_line_p.id = r_session.GetSession()->GetId();
					clear_line_p.score = r_session.GetScore();
					clear_line_p.line_index = index_lines[i];
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
			r_session.SetRoomUserState(ROOM_USER_STATE::WAIT);
			S2C_GAMEOVER_PACKET send_p; // 싱글은 게임오버 = 게임 끝
			send_p.size = sizeof(S2C_GAMEOVER_PACKET);
			send_p.type = S2C_GAMEOVER;
			send_p.id = r_session.GetSession()->GetId();
			r_session.AddToSendBuffer(reinterpret_cast<char*>(&send_p), send_p.size);
			r_session.GetSession()->SendBoundPacket(reinterpret_cast<char*>(&send_p), send_p.size, server->GetHandle());
			if (max_user == 1) {
				ClearGame();
			}
			else {
				if(CheckWinner()) {
					ClearGame();
				}
			}
			return;
		}
		}
	}
}

void TetrisRoom::BroadcastTickData()
{
	for (auto& r_user : room_users) {
		if (r_user.GetRoomUserState() == ROOM_USER_STATE::EMPTY) continue;
		r_user.SendTickBatch(server->GetHandle());
	}
}

void TetrisRoom::Broadcast(char* packet, const HANDLE iocp_handle)
{
	for (auto& r_user : room_users) {
		if (r_user.GetRoomUserState() == ROOM_USER_STATE::EMPTY) continue;
			r_user.GetSession()->SendPacket(packet, iocp_handle);
	}
}

void TetrisRoom::SendToSelf(char* packet, Session* session)
{
	// 굳이 필요 없는 함수인 듯 하다..?
}

void TetrisRoom::InitGame()
{
	for (auto& r_user : room_users) { 
		if (r_user.GetRoomUserState() == ROOM_USER_STATE::EMPTY) continue;
		r_user.GetTetris().Clear();
	}
}

void TetrisRoom::ClearGame()
{
	room_state = WAIT;
	tasks.Clear();
	tetromino_spawn_list.clear();
	for (auto& r_user : room_users) {
		if (r_user.GetRoomUserState() == ROOM_USER_STATE::EMPTY) continue;
		r_user.ClearData();
	}
}

void TetrisRoom::ProcessPlayTasks()
{
	tasks.SwapTask();
	while(!tasks.task_queue.IsEmpty()){
		TaskInfo task = tasks.GetTask();
		for (auto& r_user : room_users) {
			if (r_user.GetRoomUserState() == ROOM_USER_STATE::EMPTY) continue;
			if (r_user.GetSession()->GetId() == task.id) {
				// 각 작업들을 각 세션에 분배
				r_user.GetTetris().GetInputTasks().emplace_back(task.type);
				
				//r_user.GetTetris().DebugPrintBoard();
				break;
			}
		}
	}

	for(auto& r_user : room_users){
		if (r_user.GetRoomUserState() == ROOM_USER_STATE::EMPTY) continue;
		std::vector<TaskType> send_pending_tasks = r_user.GetTetris().TickProcess();
		BuildBroadcastData(r_user, send_pending_tasks);
	}


	ClearEventsInTick();
	BroadcastTickData();
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

bool TetrisRoom::SetNewTetromino(int id)
{
	for (auto& r_user : room_users) {
		if (r_user.GetRoomUserState() == ROOM_USER_STATE::EMPTY) continue;
		if (r_user.GetSession()->GetId() == id){
			r_user.GetTetris().InitNewTetromino((tetromino_spawn_list[r_user.GetTetrominoIndex()]), spawn_pos);
			return true;
		}
	}

	return false;
}

bool TetrisRoom::CheckWinner()
{
	if (max_user == 1) {
		//for (auto& r_user : room_users) {

		//}
		return false;
	}
	else {
		int over_count = 0;
		int player_count = 0;
		for (auto& r_user : room_users){
			if (r_user.GetRoomUserState() == ROOM_USER_STATE::PLAY) ++player_count;
			if (r_user.GetRoomUserState() == ROOM_USER_STATE::GAMEOVER) ++over_count;
		}

		if ((player_count - over_count) > 1) return false; // 아직 승자가 결정되지 않음
		else {
			for (auto& r_user : room_users) {
				// 승자와 패자는 다른 패킷이 가야함(승자는 승리, 패자는 패배 플래그)
				bool is_winner = false;
				if (r_user.GetRoomUserState() == ROOM_USER_STATE::GAMEOVER) is_winner = false;
				else is_winner = true;
				S2C_GAMEEND_PACKET gameend_p;
				gameend_p.size = sizeof(S2C_GAMEEND_PACKET);
				gameend_p.type = S2C_GAMEEND;
				gameend_p.id = r_user.GetSession()->GetId();
				gameend_p.is_winner = is_winner;
				Broadcast(reinterpret_cast<char*>(&gameend_p), server->GetHandle());
			}
			return true;
		}
	}
}

void TetrisRoom::ClearRoom()
{
	room_state = EMPTY;
	tasks.Clear();
	tetromino_spawn_list.clear();
	room_users.clear();
}

void TetrisRoom::UpdateTick()
{
	for(auto& r_user : room_users){
		if (r_user.GetRoomUserState() == ROOM_USER_STATE::EMPTY) continue;
		r_user.GetTetris().GetTickData().UpdateTickData();
	}
}

void TetrisRoom::ReduceTimeouts(int type, RoomSession& r_session)
{
	switch(type){
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

void TetrisRoom::MakeMovePacketData(RoomSession& r_session, int move_type)
{
	S2C_MOVE_PACKET move_p;
	move_p.size = sizeof(S2C_MOVE_PACKET);
	move_p.type = S2C_MOVE;
	move_p.id = r_session.GetSession()->GetId();
	move_p.move_type = static_cast<char>(move_type);
	r_session.AddToSendBuffer(reinterpret_cast<char*>(&move_p), move_p.size);
}

void TetrisRoom::ClearEventsInTick()
{
	for(auto& r_user : room_users){
		if (r_user.GetRoomUserState() == ROOM_USER_STATE::EMPTY) continue;
		r_user.GetTetris().ClearPendingMoves();
		r_user.GetTetris().ClearTasks();
	}
}

void TetrisRoom::SetRoomId(int room_index)
{
	room_id = room_index;
}

void TetrisRoom::SetRoomState(ROOM_STATE new_state)
{
	room_state = new_state;
}

int TetrisRoom::FindNewHost(int delete_id)
{
	for (auto& r_user : room_users) { 
		if (r_user.GetRoomUserState() != ROOM_USER_STATE::EMPTY) {
			if (r_user.GetSession()->GetId() == delete_id) continue;
			return r_user.GetSession()->GetId();
		}
	}
	return -1;
}

