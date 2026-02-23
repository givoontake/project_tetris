#include <random>
#include "TetrisRoom.h"
#include "IOCPServer.h"

TetrisRoom::TetrisRoom(IOCPServer* _server, Session* session, char _max_user, char _room_name[MAX_ROOM_NAME], char _room_password[MAX_ROOM_PASSWORD])
{
	server = _server;
	max_user = _max_user;
	memcpy(this->room_name, _room_name, sizeof(this->room_name));
	room_password = new char[MAX_ROOM_PASSWORD];
	memcpy(room_password, _room_password, MAX_ROOM_PASSWORD);
	room_users.reserve(max_user); // 미리 메모리를 할당하고 객체를 채우면 문제 x
	for (int i = 0; i < max_user; ++i) {
		room_users.emplace_back();
	}
	room_users[0].InitSession(session);
}

TetrisRoom::TetrisRoom(IOCPServer* _server, Session* session, char _max_user, char _room_name[MAX_ROOM_NAME])
{
	server = _server;
	max_user = _max_user;
	memcpy(this->room_name, _room_name, sizeof(this->room_name));
	room_password = nullptr;
	room_users.reserve(max_user); // 미리 메모리를 할당하고 객체를 채우면 문제 x
	for (int i = 0; i < max_user; ++i) {
		room_users.emplace_back();
	}
	room_users[0].InitSession(session);
}

TetrisRoom::~TetrisRoom()
{
	if (room_password) delete[] room_password;
}

void TetrisRoom::SendAddRoom(Session* session) // 네트워크 절약을 위해 비밀번호 포함 여부를 다르게 하여 패킷을 두 개로 구분, 같은 역할이므로 하나의 함수로 받아 케이스로 처리
{
	if (!room_password) {
		S2C_ADD_OPEN_ROOM_PACKET open_p;
		open_p.size = sizeof(S2C_ADD_OPEN_ROOM_PACKET);
		open_p.type = S2C_ADD_OPEN_ROOM;
		open_p.id = session->GetId();
		open_p.max_user = max_user;
		memcpy(open_p.room_name, room_name, sizeof(room_name));
		session->SendPacket(reinterpret_cast<char*>(&open_p), server->GetHandle());
	}
	else {
		S2C_ADD_LOCK_ROOM_PACKET lock_p;
		lock_p.size = sizeof(S2C_ADD_LOCK_ROOM_PACKET);
		lock_p.type = S2C_ADD_LOCK_ROOM;
		lock_p.id = session->GetId();
		lock_p.max_user = max_user;
		memcpy(lock_p.room_name, room_name, sizeof(room_name));
		memcpy(lock_p.room_password, room_password, MAX_ROOM_PASSWORD);
		session->SendPacket(reinterpret_cast<char*>(&lock_p), server->GetHandle());
	}
	std::cout << "Session id: " << session->GetId() << " Create room. room id(index): " << room_id;
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
	room_state = ROOM_STATE::WAIT;
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
		BoundPackets(r_user, send_pending_tasks);
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

void TetrisRoom::ClearRoom()
{
	room_state = ROOM_STATE::EMPTY;
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

void TetrisRoom::MakeMovePacket(RoomSession& r_session, int move_type)
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

void TetrisRoom::SetRoomIndex(const int val)
{
	room_index = val;
}

void TetrisRoom::SetRoomId(const int val)
{
	room_id = val;
}

void TetrisRoom::SetRoomState(ROOM_STATE new_state)
{
	room_state.Store(new_state);
}
