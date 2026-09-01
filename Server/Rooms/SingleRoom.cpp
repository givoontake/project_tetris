#include <random>
#include <algorithm>
#include "SingleRoom.h"
#include "packet_types.h"

SingleRoom::SingleRoom(IOCPServer* server, OpenRoomInitData data)
	: TetrisRoom(server, data)
{
	max_user = 1;
}

SingleRoom::SingleRoom(IOCPServer* server, LockRoomInitData data)
	: TetrisRoom(server, data)
{
	max_user = 1;
}

std::span<RoomSession> SingleRoom::GetRoomUsers()
{
	return room_users;
}

void SingleRoom::HandlePacket(char* packet, const SP<Session>& request_session)
{
	if (!request_session) return;
	switch (reinterpret_cast<PacketHeader*>(packet)->type) {
	case C2S_GIVEUP: {
		RoomTaskInfo task;
		task.type = ROOM_TASK_TYPE::GIVEUP;
		task.session = request_session;
		AddRoomTask(std::move(task));
		break;
	}
	default:
		TetrisRoom::HandlePacket(packet, request_session);
		break;
	}
}

void SingleRoom::GiveUp(const SP<Session>& request_session)
{
	if (!IsRoomSession(request_session)) return;
	if (room_state == ROOM_STATE::PLAY) {
		auto session = room_users[0].GetSession();
		if (!session || session != request_session) return;
		S2C_GAMEOVER_PACKET gameover_p;
		gameover_p.header.size = static_cast<std::uint16_t>(sizeof(gameover_p));
		gameover_p.header.type = S2C_GAMEOVER;
		gameover_p.id = session->GetDBInfo().id;
		session->SendPacket(reinterpret_cast<char*>(&gameover_p), server->GetHandle());
		RequestUpdateScore();
		ClearGame();
	}
}

void SingleRoom::ProcessSpecificRoomTask(const RoomTaskInfo& task)
{
	switch (task.type) {
	case ROOM_TASK_TYPE::START:
		if (IsRoomSession(task.session)) StartGame();
		break;
	case ROOM_TASK_TYPE::GIVEUP:
		GiveUp(task.session);
		break;
	default:
		break;
	}
}

void SingleRoom::ProcessGameTick()
{
	for (auto& r_user : room_users) {
		auto session = r_user.GetSession();
		if (!session) continue;
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
	int failed_user_id = BoundPackets();
	if (failed_user_id != -1) {
		DeleteUser(failed_user_id);
		TryPostRoomDelete();
		return;
	}

	ResetUsersTickData();
	BroadcastTickDataForUsers();
	if (is_over) {
		RequestUpdateScore();
		ClearGame();
	}
}

void SingleRoom::StartGame()
{
	{
		if (room_state == ROOM_STATE::PLAY) return;
		if (!TryChangeRoomState(ROOM_STATE::WAIT, ROOM_STATE::PLAY)) return;
		play_generation.fetch_add(1);

		// 모든 조건 통과->게임 시작
		InitGame();
		Add7BagTetrominoList();
		for (auto& r_user : room_users) {
			auto session = r_user.GetSession();
			if (!session) continue;
			r_user.SetRoomUserState(ROOM_USER_STATE::PLAY);
			r_user.GetTetris().InitNewTetromino(tetromino_spawn_list[r_user.GetTetrominoIndex()], spawn_pos);
		}

		S2C_SINGLE_START_PACKET start_p;
		start_p.header.size = static_cast<std::uint16_t>(sizeof(start_p));
		start_p.header.type = S2C_SINGLE_START;
		start_p.score = 0;
		Broadcast(reinterpret_cast<char*>(&start_p), server->GetHandle());

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
}

void SingleRoom::DeleteUser(const int id)
{
	//std::cout << "delete user id: " << id << std::endl;

	{
		for (auto& r_user : room_users) {
			auto session = r_user.GetSession();
			if (!session) continue;
			if (session->GetDBInfo().id == id) { // 삭제할 아이디 검색
				//std::cout << "delete user id: " << id << std::endl;
				session->SetRoomSnapShot(MODE_STATE::LOBBY, -1);
				S2C_DELETE_USER_PACKET p;
				p.header.size = static_cast<std::uint16_t>(sizeof(p));
				p.header.type = S2C_DELETE_USER;
				p.id = id;

				Broadcast(reinterpret_cast<char*>(&p), server->GetHandle());

				ClearRoom(); // 안하면 방 삭제 포스트 이후 세션이 재사용되면 문제가 될 수 있음.
				BeginRoomDelete();
				break;
			}
		}
	}
}

void SingleRoom::SendCreateRoom(const SP<Session>& session) // 외부에서 세션락 걸고 들어온다
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
	std::cout << "방 생성 - 방 이름: " << room_name << ", 플레이어: " << session->GetDBInfo().nickname << std::endl;
}

void SingleRoom::ReduceTimeouts(int type)
{
	RoomSession& r_session = room_users[0];
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
	auto session = room_users[0].GetSession();
	if (!session) return;
	S2C_MOVE_PACKET move_p;
	move_p.header.size = static_cast<std::uint16_t>(sizeof(move_p));
	move_p.header.type = S2C_MOVE;
	move_p.id = session->GetDBInfo().id;
	move_p.move_type = static_cast<char>(move_type);
	if (!room_users[0].AddToSendBuffer(reinterpret_cast<char*>(&move_p), move_p.header.size)) {
		DeleteUser(session->GetDBInfo().id);
		TryPostRoomDelete();
	}
}

void SingleRoom::RequestUpdateScore()
{
	auto session_shared = room_users[0].GetSession();
	if (!session_shared) return;
	if (session_shared->GetDBInfo().max_score < room_users[0].GetScore()) {
		int new_score = room_users[0].GetScore();
		SessionKey key = session_shared->GetSessionKey();
		server->EnqueueDBTask(std::make_unique<DBUpdateScoreTask>(key, new_score), session_shared);
	}
}
