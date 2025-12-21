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

void TetrisRoom::BuildBroadcastData(const char* data, int data_size)
{
	for (auto& r_user : room_users) {
		if (r_user.GetRoomUserState() == ROOM_USER_STATE::EMPTY) continue;
		r_user.AddToSendBuffer(data, data_size);
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
	CheckAddGarbageLineTimeout();
	CheckMoveDownTimeout();
	tasks.SwapTask();
	while(!tasks.task_queue.IsEmpty()){
		TaskInfo task = tasks.GetTask();
		for (auto& r_user : room_users) {
			if (r_user.GetRoomUserState() == ROOM_USER_STATE::EMPTY) continue;
			if (r_user.GetSession()->GetId() == task.id) {
				r_user.GetTetris().SetPendingMove(task.type);
				
				//r_user.GetTetris().DebugPrintBoard();
				break;
			}
		}
	}

	for(auto& r_user : room_users){
		if (r_user.GetRoomUserState() == ROOM_USER_STATE::EMPTY) continue;
		std::vector<char> send_tasks = r_user.GetTetris().ProcessPendingMoves();
		
		for (auto& type : send_tasks) {
			switch (type) {
			case -1:
				break;

			case RIGHT:
				MakeMovePacketData(r_user.GetSession()->GetId(), type);
				break;

			case LEFT:
				MakeMovePacketData(r_user.GetSession()->GetId(), type);
				break;

			case DOWN:
				r_user.SetDownTick(0);
				MakeMovePacketData(r_user.GetSession()->GetId(), type);
				break;

			case ROTATE:
				MakeMovePacketData(r_user.GetSession()->GetId(), type);
				break;

			case DROP:
				r_user.GetTetris().FixTetromino();
				r_user.SetDownTick(0);
				if (r_user.GetTetris().CheckGameover()) { // 고정에 성공했다면 게임오버 판정
					if (max_user > 1) {
						if (CheckWinner()) { // 누군가 게임오버였다면, 승자 여부 추가확인
							ClearGame(); // 승자가 나왔다면 방 상태 정리(클리어는 아님)하고 함수 종료
							return;
						}
						r_user.SetRoomUserState(ROOM_USER_STATE::GAMEOVER);
					}
					r_user.SetRoomUserState(ROOM_USER_STATE::WAIT);
					S2C_GAMEOVER_PACKET send_p; // 싱글은 게임오버 = 게임 끝
					send_p.size = sizeof(S2C_GAMEOVER_PACKET);
					send_p.type = S2C_GAMEOVER;
					send_p.id = r_user.GetSession()->GetId();
					BuildBroadcastData(reinterpret_cast<char*>(&send_p), send_p.size);

					ClearGame();
				}

				S2C_FIX_PACKET fix_p;
				fix_p.size = sizeof(S2C_FIX_PACKET);
				fix_p.type = S2C_FIX;
				fix_p.id = r_user.GetSession()->GetId();
				fix_p.fixed_x = r_user.GetTetris().GetCurrentTetromino().moved_pos.x;
				fix_p.fixed_y = r_user.GetTetris().GetCurrentTetromino().moved_pos.y;
				BuildBroadcastData(reinterpret_cast<char*>(&fix_p), fix_p.size);

				std::vector<char> index_lines = r_user.GetTetris().ClearLine();
				if (!index_lines.empty()) {
					r_user.SetScore(r_user.GetScore() + (CLEAR_LINE_SCORE * index_lines.size() * index_lines.size()));
					for (int i = 0; i < index_lines.size(); i++) {
						S2C_CLEARLINE_PACKET clear_line_p;
						clear_line_p.size = sizeof(S2C_CLEARLINE_PACKET);
						clear_line_p.type = S2C_CLEARLINE;
						clear_line_p.id = r_user.GetSession()->GetId();
						clear_line_p.score = r_user.GetScore();
						clear_line_p.line_index = index_lines[i];
						BuildBroadcastData(reinterpret_cast<char*>(&clear_line_p), clear_line_p.size);
					}
				}

				if (r_user.GetTetrominoIndex() == tetromino_spawn_list.size() - 2) Add7BagTetrominoList();
				r_user.AddTetrominoIndex();

				if (SetNewTetromino(r_user.GetSession()->GetId())) {
					S2C_SPAWN_PACKET spawn_p;
					spawn_p.size = sizeof(S2C_SPAWN_PACKET);
					spawn_p.type = S2C_SPAWN;
					spawn_p.id = r_user.GetSession()->GetId();
					spawn_p.tetromino_type = tetromino_spawn_list[r_user.GetTetrominoIndex()];
					spawn_p.next_tetromino_type = tetromino_spawn_list[r_user.GetTetrominoIndex() + 1];
					spawn_p.spawn_x = spawn_pos.x;
					spawn_p.spawn_y = spawn_pos.y;
					BuildBroadcastData(reinterpret_cast<char*>(&spawn_p), spawn_p.size);
				}
				break;
			}
		}
	}
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
		//	if (!r_user.GetInUse()) continue;
		//	if (r_user.GetIsOver()) {
		//		S2C_GAMEOVER_PACKET send_p;
		//		send_p.size = sizeof(S2C_GAMEOVER_PACKET);
		//		send_p.type = S2C_GAMEOVER;
		//		send_p.id = r_user.GetSession()->GetId();
		//	}
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
				if (r_user.GetRoomUserState() == ROOM_USER_STATE::GAMEOVER) {
					S2C_GAMEEND_PACKET gameend_p;
					gameend_p.size = sizeof(S2C_GAMEEND_PACKET);
					gameend_p.type = S2C_GAMEEND;
					gameend_p.winner_id = r_user.GetSession()->GetId();
					BuildBroadcastData(reinterpret_cast<char*>(&gameend_p), gameend_p.size);
				}
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
		r_user.SetDownTick(r_user.GetDownTick() + 1);
		//r_user.SetInputTick(r_user.GetInputTick() + 1);
		r_user.SetAddGarbageLineTick(r_user.GetAddGarbageLineTick() + 1);
	}
}

void TetrisRoom::CheckMoveDownTimeout()
{
	for(auto& r_user : room_users){
		if (r_user.GetRoomUserState() == ROOM_USER_STATE::EMPTY) continue;
		if (r_user.GetDownTick() >= r_user.GetDownTimeout()) {
			TaskInfo new_task;
			new_task.id = r_user.GetSession()->GetId();
			new_task.type = DOWN;
			tasks.AddTask(new_task);
			ReduceTimeouts(DOWN, r_user);
			r_user.SetDownTick(0);
		}
	}
}

void TetrisRoom::CheckAddGarbageLineTimeout()
{
	for (auto& r_user : room_users) {
		if (r_user.GetRoomUserState() == ROOM_USER_STATE::EMPTY) continue;
		if (r_user.GetAddGarbageLineTick() < r_user.GetAddGarbageLineTimeout()) continue;
		r_user.SetAddGarbageLineTick(0);
		ReduceTimeouts(ADD_TIMEOUT, r_user); // 다음 타임아웃 재설정
		std::vector<char> holes = r_user.GetTetris().GetGarbegeLineHoles(2); // 타임아웃 나는건 싱글뿐이라 1칸 추가인 2를 넘김
		std::vector<char> tasks_from_add_garbege_lines = r_user.GetTetris().AddGarbageLines(holes);
		if (!tasks_from_add_garbege_lines.empty()) {
			std::vector<char> garbage_line_holes;
			for (auto& task_type : tasks_from_add_garbege_lines) {
				// 패킷 전달 순서는 이전 함수에서 작업 순서대로 등록함. 그냥 작업타입 받아서 패킷 전송 준비만 하면 된다.

				if (task_type >= 0) {
					S2C_ADDLINE_PACKET add_line_p;
					add_line_p.size = sizeof(S2C_ADDLINE_PACKET);
					add_line_p.type = S2C_ADDLINE;
					add_line_p.id = r_user.GetSession()->GetId();
					add_line_p.hole_x = task_type;
					BuildBroadcastData(reinterpret_cast<char*>(&add_line_p), add_line_p.size);
				}

				else if (task_type == FIX) {
					S2C_FIX_PACKET fix_p;
					fix_p.size = sizeof(S2C_FIX_PACKET);
					fix_p.type = S2C_FIX;
					fix_p.id = r_user.GetSession()->GetId();
					fix_p.fixed_x = r_user.GetTetris().GetCurrentTetromino().moved_pos.x;
					fix_p.fixed_y = r_user.GetTetris().GetCurrentTetromino().moved_pos.y;
					BuildBroadcastData(reinterpret_cast<char*>(&fix_p), fix_p.size);
					r_user.SetDownTick(0);
					//r_user.SetInputTick(0);
				}

				else if (task_type == SPAWN) {
					S2C_SPAWN_PACKET spawn_p;
					spawn_p.size = sizeof(S2C_SPAWN_PACKET);
					spawn_p.type = S2C_SPAWN;
					spawn_p.id = r_user.GetSession()->GetId();
					spawn_p.tetromino_type = tetromino_spawn_list[r_user.GetTetrominoIndex()];
					spawn_p.next_tetromino_type = tetromino_spawn_list[r_user.GetTetrominoIndex() + 1];
					spawn_p.spawn_x = spawn_pos.x;
					spawn_p.spawn_y = spawn_pos.y;
					BuildBroadcastData(reinterpret_cast<char*>(&spawn_p), spawn_p.size);
				}

				else if (task_type == GAMEOVER) {
					S2C_GAMEOVER_PACKET gameover_p;
					gameover_p.size = sizeof(S2C_GAMEOVER_PACKET);
					gameover_p.type = S2C_GAMEOVER;
					gameover_p.id = r_user.GetSession()->GetId();
					BuildBroadcastData(reinterpret_cast<char*>(&gameover_p), gameover_p.size);
				}
			}
		}
	}
}

//bool TetrisRoom::CheckInputTick(RoomSession& r_session)
//{
//	if (r_session.GetInputTick() >= INPUT_TICK) {
//		return true;
//	}
//
//	return false;
//}

void TetrisRoom::ReduceTimeouts(int type, RoomSession& r_session)
{
	switch(type){
	case DOWN_TIMEOUT:
		if (r_session.GetScore() <= 100) {
			r_session.SetDownTimeout(25);
		}

		else if (100 < r_session.GetScore() && r_session.GetScore() <= 300) {
			r_session.SetDownTimeout(24);
		}

		else if (300 < r_session.GetScore() && r_session.GetScore() <= 600) {
			r_session.SetDownTimeout(23);
		}

		else if (600 < r_session.GetScore() && r_session.GetScore() <= 1000) {
			r_session.SetDownTimeout(22);
		}

		else if (1000 < r_session.GetScore() && r_session.GetScore() <= 1500) {
			r_session.SetDownTimeout(21);
		}

		else if (1500 < r_session.GetScore()) {
			r_session.SetDownTimeout(20);
		}
		
		break;

	case ADD_TIMEOUT:
		r_session.SetAddGarbageLineTimeout(r_session.GetAddGarbageLineTimeout() - 1);
		break;
	}
}

void TetrisRoom::MakeMovePacketData(int id, int move_type)
{
	S2C_MOVE_PACKET move_p;
	move_p.size = sizeof(S2C_MOVE_PACKET);
	move_p.type = S2C_MOVE;
	move_p.id = id;
	move_p.move_type = static_cast<char>(move_type);
	BuildBroadcastData(reinterpret_cast<char*>(&move_p), move_p.size);
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

