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
		if (r_user.GetInUse()) continue;
			
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
	p.id = new_session->GetIndex();
	p.is_add = false;
	new_session->SendPacket(reinterpret_cast<char*>(&p), server->GetHandle()); // 방이 꽉 찼을 경우 본인에게만 실패 전송
}

void TetrisRoom::DeleteUser(const int id)
{
	//std::cout << "delete user id: " << id << std::endl;
	for (auto& r_user : room_users){
		if (!r_user.GetInUse()) continue;
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
			r_user.SetIsReady();

			S2C_READY_PACKET p;
			p.size = sizeof(S2C_READY_PACKET);
			p.type = S2C_READY;
			p.id = r_user.GetSession()->GetIndex();
			p.is_ready = r_user.GetIsReady();
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
			r_user.SetUse(true, false);
			r_user.GetSession()->SetState(LOBBY);
			r_user.ClearSession(); // 해당 아이디 세션 정리

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
		if (!r_user.GetInUse()) continue; // 사용 중이지 않은 인덱스는 건너뜀
		if (r_user.GetSession()->GetIndex() == host_id) continue; // 방장은 건너뜀

		if (!r_user.GetIsReady()) { // 방에 있는데 레디가 안된 사람이 있으면 시작 불가			
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

		room_users[host_index].GetSession()->SendPacket(reinterpret_cast<char*>(&start_p), server->GetHandle()); // 시작 불가는 방장에게만 보내면 됨
		// 보내야 할까? 시작 불가 알림창 정도는  클라에게 맏겨도 될 듯 하다. 잘못 와도 시작만 안하면 되니까
		return;
	}

	// 모든 조건 통과->게임 시작
	Add7BagTetrominoList();
	for (auto& r_user : room_users){
		if (!r_user.GetInUse()) continue;
		r_user.GetTetris().InitNewTetromino(tetromino_spawn_list[r_user.GetTetrominoIndex()], spawn_pos);
	}
	SetRoomState(PLAY);

	// 테트리스 게임 중에 들어오는 패킷은 또 따로 분리하고 싶기는 한데..
	InitGame();

	start_p.size = sizeof(S2C_START_PACKET);
	start_p.type = S2C_START;
	start_p.is_start = true;
	Broadcast(reinterpret_cast<char*>(&start_p), server->GetHandle());
	
	for (auto& r_user : room_users) {
		if (!r_user.GetInUse()) continue;
		S2C_SPAWN_PACKET spawn_p;
		spawn_p.size = sizeof(S2C_SPAWN_PACKET);
		spawn_p.type = S2C_SPAWN;
		spawn_p.id = r_user.GetSession()->GetId();
		spawn_p.tetromino_type = tetromino_spawn_list[r_user.GetTetrominoIndex()];
		spawn_p.spawn_x = spawn_pos.x;
		spawn_p.spawn_y = spawn_pos.y;
		spawn_p.fixed_x = -1;
		spawn_p.fixed_y = -1;
		Broadcast(reinterpret_cast<char*>(&spawn_p), server->GetHandle());
	}

}

void TetrisRoom::Broadcast(char* packet, const HANDLE iocp_handle)
{
	for (auto& r_user : room_users) {
		if (r_user.GetInUse()) {
			r_user.GetSession()->SendPacket(packet, iocp_handle);
		}
	}
}

void TetrisRoom::SendToSelf(char* packet, Session* session)
{
	// 굳이 필요 없는 함수인 듯 하다..?
}

void TetrisRoom::InitGame()
{
	for (auto& r_user : room_users) { 
		if (!r_user.GetInUse()) continue;
		r_user.GetTetris().Clear();
	}
}

void TetrisRoom::ClearGame()
{
	room_state = WAIT;
	tasks.Clear();
	tetromino_spawn_list.clear();
	for (auto& r_user : room_users) {
		if (!r_user.GetInUse()) continue;
		r_user.ClearData();
	}
}

void TetrisRoom::ProcessPlayTasks()
{
	tasks.SwapTask();
	while(!tasks.task_queue.IsEmpty()){
		TaskInfo task = tasks.GetTask();
		for (auto& r_user : room_users) {
			if (!r_user.GetInUse()) continue;
			if (r_user.GetIsOver()) continue;
			if (r_user.GetSession()->GetId() == task.id) {
				// 테트리스 키 입력 처리
				if (r_user.GetTetris().HandleTetrominoKeyInput(task.type)) { // 착지(고정)에 성공했는가?
					if (r_user.GetTetris().CheckGameover()) { // 고정에 성공했다면 게임오버 판정
						r_user.SetIsOver(true);
						S2C_GAMEOVER_PACKET send_p;
						send_p.size = sizeof(S2C_GAMEOVER_PACKET);
						send_p.type = S2C_GAMEOVER;
						send_p.id = task.id;
						Broadcast(reinterpret_cast<char*>(&send_p), server->GetHandle());
						if (max_user > 1 && CheckWinner()) { // 누군가 게임오버였다면, 승자 여부 추가확인
							ClearGame(); // 승자가 나왔다면 방 상태 정리(클리어는 아님)하고 함수 종료
							return;
						}
					}

					else {
						std::vector<char> index_lines = r_user.GetTetris().ClearLine();
						if (!index_lines.empty()) {
							S2C_CLEARLINE_PACKET clear_line_p;
							int index_size = index_lines.size();
							int total_size = sizeof(S2C_CLEARLINE_PACKET) + index_size;
							char* send_p = new char[total_size];
							clear_line_p.size = total_size;
							clear_line_p.type = S2C_CLEARLINE;
							clear_line_p.id = task.id;
							memcpy(send_p, reinterpret_cast<char*>(&clear_line_p), sizeof(S2C_CLEARLINE_PACKET));
							memcpy(send_p + sizeof(S2C_CLEARLINE_PACKET), index_lines.data(), index_size);
							Broadcast(send_p, server->GetHandle());

							delete[] send_p;
						}
					}

					if (r_user.GetTetrominoIndex() == tetromino_spawn_list.size() - 1) Add7BagTetrominoList(); 
					Tetromino prev_tetromino = r_user.GetTetris().GetCurrentTetromino();
					r_user.AddTetrominoIndex();

					if (SetNewTetromino(task.id)) {
						S2C_SPAWN_PACKET spawn_p;
						spawn_p.size = sizeof(S2C_SPAWN_PACKET);
						spawn_p.type = S2C_SPAWN;
						spawn_p.id = r_user.GetSession()->GetId();
						spawn_p.tetromino_type = tetromino_spawn_list[r_user.GetTetrominoIndex()];
						spawn_p.spawn_x = spawn_pos.x;
						spawn_p.spawn_y = spawn_pos.y;
						spawn_p.fixed_x = prev_tetromino.moved_pos.x;
						spawn_p.fixed_y = prev_tetromino.moved_pos.y;
						Broadcast(reinterpret_cast<char*>(&spawn_p), server->GetHandle());
					}

				}

				else {
					if (r_user.GetTetris().GetMoveAllow()) {
						S2C_MOVE_PACKET send_p;
						send_p.size = sizeof(S2C_MOVE_PACKET);
						send_p.type = S2C_MOVE;
						send_p.id = task.id;
						send_p.move_type = task.type;
						Broadcast(reinterpret_cast<char*>(&send_p), server->GetHandle());
						r_user.GetTetris().SetMoveAllow(false);
					}
				}			
				r_user.GetTetris().GetCurrentTetromino().PrintInfo();
				r_user.GetTetris().DebugPrintBoard();
				break;
			}
		}
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

bool TetrisRoom::SetNewTetromino(int id)
{
	for (auto& r_user : room_users) {
		if (!r_user.GetInUse()) continue;
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
			if (r_user.GetInUse()) ++player_count;
			if (r_user.GetInUse() && r_user.GetIsOver()) ++over_count;
		}

		if ((player_count - over_count) > 1) return false; // 아직 승자가 결정되지 않음
		else {
			for (auto& r_user : room_users) {
				if (r_user.GetInUse() && !r_user.GetIsOver()) {
					S2C_GAMEEND_PACKET send_p;
					send_p.size = sizeof(S2C_GAMEEND_PACKET);
					send_p.type = S2C_GAMEEND;
					send_p.winner_id = r_user.GetSession()->GetId();
					Broadcast(reinterpret_cast<char*>(&send_p), server->GetHandle());
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
		if (r_user.GetInUse()) {
			if (r_user.GetSession()->GetId() == delete_id) continue;
			return r_user.GetSession()->GetId();
		}
	}
	return -1;
}

