#include <random>
#include <algorithm>
#include "SingleRoom.h"
#include "packet_types.h"

SingleRoom::SingleRoom(IOCPServer* server, PublicRoomInitData data)
	: TetrisRoom(server, data)
{
	max_player_count_ = 1;
}

SingleRoom::SingleRoom(IOCPServer* server, PrivateRoomInitData data)
	: TetrisRoom(server, data)
{
	max_player_count_ = 1;
}

std::span<Player> SingleRoom::GetRoomPlayers()
{
	return room_players_;
}

void SingleRoom::HandlePacket(char* packet, Session* request_session)
{
	if (!request_session) return;
	switch (reinterpret_cast<PACKET_HEADER*>(packet)->type) {
	case C2S_GIVE_UP: {
		RoomTask task;
		task.task_type = RoomTaskType::GIVE_UP;
		task.session_key = request_session->GetSessionKey();
		AddRoomTask(std::move(task));
		break;
	}
	default:
		TetrisRoom::HandlePacket(packet, request_session);
		break;
	}
}

void SingleRoom::GiveUp(Session* request_session)
{
	if (!IsPlayerInRoom(request_session)) return;
	if (room_state_ == RoomState::PLAY) {
		auto session = FindSession(room_players_[0]);
		if (!session || session != request_session) return;
		S2C_GAME_OVER_PACKET game_over_p;
		game_over_p.header.size = static_cast<std::uint16_t>(sizeof(game_over_p));
		game_over_p.header.type = S2C_GAME_OVER;
		game_over_p.player_id = session->GetDBInfo().player_id;
		session->SendPacket(reinterpret_cast<char*>(&game_over_p), game_over_p.header.size, server_->GetIOCPHandle());
		RequestUpdateScore();
		ClearGame();
	}
}

bool SingleRoom::ProcessSpecificRoomTask(const RoomTask& task)
{
	auto session = server_->FindSession(task.session_key);
	if (!session) return true;
	switch (task.task_type) {
	case RoomTaskType::START:
		if (IsPlayerInRoom(session)) StartGame();
		break;
	case RoomTaskType::GIVE_UP:
		GiveUp(session);
		break;
	default:
		break;
	}
	return true;
}

void SingleRoom::ProcessGameTick(long long tick_time_ms)
{
	if (!room_players_[0].IsActive()) return;
	for (auto& room_player : room_players_) {
		auto session = FindSession(room_player);
		if (!session) continue;
		room_player.GetTetris().ProcessTick(tick_time_ms);
	}

	AddGarbageLines();
	bool is_game_over = false;
	is_game_over = room_players_[0].GetTetris().CheckGameOver();
	if (!is_game_over) AddSpawnTasks();

	auto tasks = room_players_[0].GetTetris().GetSendTasks();
	auto it = std::find_if(tasks.begin(), tasks.end(), [](const TaskType& task) {
		return task.event_type == EventType::FIX;
		});

	if (it != tasks.end()) { // 고정 이벤트가 있어야 점수 및 콤보계산
		CalculateScore(room_players_[0].GetTetris().GetClearedLineCount());
	}
	SessionKey failed_session_key = AppendTickPackets();
	if (failed_session_key.session_index != -1) {
		RoomTask task;
		task.task_type = RoomTaskType::REMOVE_PLAYER;
		task.session_key = failed_session_key;
		AddRoomTask(std::move(task));
		return;
	}

	ResetPlayerTickState();
	BroadcastPackets();
	if (is_game_over) {
		RequestUpdateScore();
		ClearGame();
	}
}

void SingleRoom::StartGame()
{
	{
		if (room_state_ == RoomState::PLAY) return;
		if (!TryChangeRoomState(RoomState::WAIT, RoomState::PLAY)) return;
		play_generation_.fetch_add(1);

		// 모든 조건 통과->게임 시작
		InitGame();
		AppendTetromino7Bag();
		for (auto& room_player : room_players_) {
			auto session = FindSession(room_player);
			if (!session) continue;
			room_player.SetRoomPlayerState(RoomPlayerState::PLAY);
			room_player.GetTetris().InitNewTetromino(tetromino_spawn_list_[room_player.GetTetrominoIndex()], spawn_pos_);
		}

		S2C_SINGLE_START_PACKET start_p;
		start_p.header.size = static_cast<std::uint16_t>(sizeof(start_p));
		start_p.header.type = S2C_SINGLE_START;
		start_p.score = 0;
		Broadcast(reinterpret_cast<char*>(&start_p), server_->GetIOCPHandle());

		for (auto& room_player : room_players_) {
			auto session = FindSession(room_player);
			if (!session) continue;
			S2C_SPAWN_PACKET spawn_p;
			spawn_p.header.size = static_cast<std::uint16_t>(sizeof(spawn_p));
			spawn_p.header.type = S2C_SPAWN;
			spawn_p.player_id = session->GetDBInfo().player_id;
			spawn_p.tetromino_type = tetromino_spawn_list_[room_player.GetTetrominoIndex()];
			spawn_p.next_tetromino_type = tetromino_spawn_list_[room_player.GetTetrominoIndex() + 1];
			spawn_p.spawn_x = spawn_pos_.x;
			spawn_p.spawn_y = spawn_pos_.y;
			Broadcast(reinterpret_cast<char*>(&spawn_p), server_->GetIOCPHandle());
		}
	}
}

void SingleRoom::RemovePlayer(SessionKey session_key)
{
	auto* session = server_->FindSession(session_key);
	if (session && session->GetLifeState() == LifeState::ACTIVE) {
		RequestLobbyTransition(session_key);
		return;
	}
	CompletePlayerRemoval(session_key, RoomExitType::LEAVE);
}

void SingleRoom::CompletePlayerRemoval(SessionKey session_key, RoomExitType exit_type)
{
	//std::cout << "delete player id: " << session_key.player_id << std::endl;

	{
		for (auto& room_player : room_players_) {
			if (room_player.MatchesSessionKey(session_key)) { // 삭제할 세션 검색
				const int player_id = room_player.GetSessionKey().player_id;
				//std::cout << "delete player id: " << player_id << std::endl;
				S2C_REMOVE_PLAYER_PACKET p;
				p.header.size = static_cast<std::uint16_t>(sizeof(p));
				p.header.type = S2C_REMOVE_PLAYER;
				p.player_id = player_id;

				Broadcast(reinterpret_cast<char*>(&p), server_->GetIOCPHandle());

				ClearRoom(); // 안하면 방 삭제 포스트 이후 세션이 재사용되면 문제가 될 수 있음.
				BeginRoomDelete();
				break;
			}
		}
	}
}

void SingleRoom::SendCreateRoom(Session* session) // 외부에서 세션락 걸고 들어온다
{
	if (!session) return;
	if (room_password_.empty()) {
		S2C_ADD_PUBLIC_ROOM_PACKET public_p;
		public_p.header.size = static_cast<std::uint16_t>(sizeof(public_p));
		public_p.header.type = S2C_ADD_PUBLIC_ROOM;
		public_p.room_gen = room_gen_;
		public_p.max_player_count = max_player_count_;
		server_->StringToCharBuf(room_name_, public_p.room_name, sizeof(public_p.room_name));
		session->SendPacket(reinterpret_cast<char*>(&public_p), public_p.header.size, server_->GetIOCPHandle());
	}
	else {
		S2C_ADD_PRIVATE_ROOM_PACKET private_p;
		private_p.header.size = static_cast<std::uint16_t>(sizeof(private_p));
		private_p.header.type = S2C_ADD_PRIVATE_ROOM;
		private_p.room_gen = room_gen_;
		private_p.max_player_count = max_player_count_;
		server_->StringToCharBuf(room_name_, private_p.room_name, sizeof(private_p.room_name));
		server_->StringToCharBuf(room_password_, private_p.room_password, sizeof(private_p.room_password));
		session->SendPacket(reinterpret_cast<char*>(&private_p), private_p.header.size, server_->GetIOCPHandle());
	}
	std::cout << "방 생성 - 방 이름: " << room_name_ << ", 플레이어: " << session->GetDBInfo().nickname << std::endl;
}

void SingleRoom::CalculateScore(int clear_line_count)
{
	int added_score = 0;
	switch (clear_line_count) {
	case 0:
		room_players_[0].ResetCombo();
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
	room_players_[0].AddCombo();
	int combo = room_players_[0].GetCombo();
	added_score += combo * (added_score / 10);
	room_players_[0].AddScore(added_score);
}

void SingleRoom::RequestUpdateScore()
{
	auto session = FindSession(room_players_[0]);
	if (!session) return;
	if (session->GetDBInfo().max_score < room_players_[0].GetScore()) {
		int new_score = room_players_[0].GetScore();
		SessionKey session_key = session->GetSessionKey();
		server_->EnqueueDBTask(std::make_unique<DBUpdateScoreTask>(session_key, new_score));
	}
}
