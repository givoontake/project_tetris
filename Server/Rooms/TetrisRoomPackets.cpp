#include <cstring>
#include "TetrisRoom.h"
#include "TetrisServer.h"
#include "game_packets.h"
#include "packet_types.h"

bool TetrisRoom::ApplySendTaskState(Player& player, int player_id, const TaskType& task)
{
	switch (task.event_type) {
	case EventType::SPAWN:
		if (player.GetTetrominoIndex() == (tetromino_spawn_list_.size() - 2)) AppendTetromino7Bag();
		player.IncrementTetrominoIndex();
		return SpawnTetromino(player_id);
	case EventType::GAME_OVER:
		player.SetRoomPlayerState(RoomPlayerState::GAME_OVER);
		break;
	default:
		break;
	}
	return true;
}

bool TetrisRoom::AppendTickPacket(Player& player, int player_id, const TaskType& task)
{
	switch (task.event_type) {
	case EventType::MOVE:
		return AppendMovePacket(player, std::get<TaskMove>(task.task));
	case EventType::FIX:
		return AppendFixPacket(player, player_id, std::get<TaskFix>(task.task));
	case EventType::CLEAR_LINE:
		return AppendClearLinePacket(player, player_id, std::get<TaskClearLine>(task.task));
	case EventType::SPAWN:
		return AppendSpawnPacket(player, player_id);
	case EventType::ADD_LINE:
		return AppendAddLinePacket(player, player_id, std::get<TaskAddLine>(task.task));
	case EventType::GAME_OVER:
		return AppendGameOverPacket(player, player_id);
	case EventType::GAME_END: // 이건 사실상 멀티만 쓰므로.. 근데 이거 하나때문에 또 분리하기 좀 그렇긴 하다 분리하는게 좋긴 할 것 같지만..
		return AppendGameEndPacket(player, std::get<TaskGameEnd>(task.task));
	default:
		return true;
	}
}

bool TetrisRoom::AppendMovePacket(Player& player, const TaskMove& task)
{
	auto* session = FindSession(player);
	if (!session) return true;
	// 각 이동의 다음 입력 허용 시각은 Tetris::ProcessMoveInput에서 갱신한다.
	S2C_MOVE_PACKET move_p;
	move_p.header.size = static_cast<std::uint16_t>(sizeof(move_p));
	move_p.header.type = S2C_MOVE;
	move_p.player_id = session->GetDBInfo().player_id;
	move_p.move_type = static_cast<char>(task.move_type);
	return player.AddToSendBuffer(reinterpret_cast<char*>(&move_p), move_p.header.size);
}

bool TetrisRoom::AppendFixPacket(Player& player, int player_id, const TaskFix& task)
{
	S2C_FIX_PACKET fix_p;
	fix_p.header.size = static_cast<std::uint16_t>(sizeof(fix_p));
	fix_p.header.type = S2C_FIX;
	fix_p.player_id = player_id;
	fix_p.fixed_x = task.fixed_x; // 실시간 반영된 값을 읽는게 아니라 작업 목록을 가져와서 패킷을 구성하므로, 작업 당시의 값을 가져와야 함. addline과 동시 틱에 처리되면 클라는 공중에 떠 있는 것으로 보이는 버그 발생
	fix_p.fixed_y = task.fixed_y;
	return player.AddToSendBuffer(reinterpret_cast<char*>(&fix_p), fix_p.header.size);
}

bool TetrisRoom::AppendClearLinePacket(Player& player, int player_id, const TaskClearLine& task)
{
	S2C_CLEAR_LINE_PACKET clear_line_p;
	clear_line_p.header.size = static_cast<std::uint16_t>(sizeof(clear_line_p));
	clear_line_p.header.type = S2C_CLEAR_LINE;
	clear_line_p.player_id = player_id;
	clear_line_p.score = player.GetScore();
	clear_line_p.line_index = task.line_index;
	clear_line_p.combo = player.GetCombo();
	return player.AddToSendBuffer(reinterpret_cast<char*>(&clear_line_p), clear_line_p.header.size);
}

bool TetrisRoom::AppendSpawnPacket(Player& player, int player_id)
{
	S2C_SPAWN_PACKET spawn_p;
	spawn_p.header.size = static_cast<std::uint16_t>(sizeof(spawn_p));
	spawn_p.header.type = S2C_SPAWN;
	spawn_p.player_id = player_id;
	spawn_p.tetromino_type = tetromino_spawn_list_[player.GetTetrominoIndex()];
	spawn_p.next_tetromino_type = tetromino_spawn_list_[player.GetTetrominoIndex() + 1];
	spawn_p.spawn_x = static_cast<char>(spawn_pos_.x);
	spawn_p.spawn_y = static_cast<char>(spawn_pos_.y);
	return player.AddToSendBuffer(reinterpret_cast<char*>(&spawn_p), spawn_p.header.size);
}

bool TetrisRoom::AppendAddLinePacket(Player& player, int player_id, const TaskAddLine& task)
{
	S2C_ADD_LINE_PACKET add_line_p;
	add_line_p.header.size = static_cast<std::uint16_t>(sizeof(add_line_p));
	add_line_p.header.type = S2C_ADD_LINE;
	add_line_p.player_id = player_id;
	add_line_p.hole_x = static_cast<char>(task.hole_x);
	return player.AddToSendBuffer(reinterpret_cast<char*>(&add_line_p), add_line_p.header.size);
}

bool TetrisRoom::AppendGameOverPacket(Player& player, int player_id)
{
	// 일단 종료 패킷을 보냄
	S2C_GAME_OVER_PACKET game_over_p;
	game_over_p.header.size = static_cast<std::uint16_t>(sizeof(game_over_p));
	game_over_p.header.type = S2C_GAME_OVER;
	game_over_p.player_id = player_id;
	return player.AddToSendBuffer(reinterpret_cast<char*>(&game_over_p), game_over_p.header.size);
}

bool TetrisRoom::AppendGameEndPacket(Player& player, const TaskGameEnd& task)
{
	S2C_GAME_END_PACKET game_end_p;
	game_end_p.header.size = static_cast<std::uint16_t>(sizeof(game_end_p));
	game_end_p.header.type = S2C_GAME_END;
	game_end_p.winner_id = task.winner_id;
	return player.AddToSendBuffer(reinterpret_cast<char*>(&game_end_p), game_end_p.header.size);
}

// 순차적으로 쌓인 작업을 처리해 보내야 할 패킷을 플레이어별 버퍼에 쌓는다.
SessionKey TetrisRoom::AppendTickPackets()
{
	auto room_players = GetRoomPlayers();
	for (auto& room_player : room_players) {
		if (room_player.GetRoomPlayerState() != RoomPlayerState::PLAY) continue; // 게임오버 되어도 상태 변경은 여기서 이루어지므로 진입 시에는 게임오버 상태는 아님
		auto* session = FindSession(room_player);
		if (!session) continue;
		const SessionKey session_key = session->GetSessionKey();
		int player_id = session->GetDBInfo().player_id;

		for (auto& task : room_player.GetTetris().GetSendTasks()) {
			if (!ApplySendTaskState(room_player, player_id, task)) continue;
			if (!AppendTickPacket(room_player, player_id, task)) return session_key;
		}
	}
	return {};
}

void TetrisRoom::BroadcastPackets()
{
	auto room_players = GetRoomPlayers();
	char send_buffer[BUF_SIZE];
	int send_data_size = 0;

	for (int i = 0; i < room_players.size(); ++i) {
		Player& source = room_players[i];
		auto source_session = FindSession(source);
		if (!source_session) continue;
		const int source_data_size = source.GetSendDataSize();
		if (source_data_size <= 0) continue;

		if (send_data_size + source_data_size > BUF_SIZE) {
			for (auto& target : room_players) {
				auto target_session = FindSession(target);
				if (!target_session) continue;
				target_session->SendPacket(send_buffer, send_data_size);
			}
			send_data_size = 0;
		}

		memcpy(send_buffer + send_data_size, source.GetSendBuffer(), source_data_size);
		send_data_size += source_data_size;
	}

	if (send_data_size > 0) {
		for (auto& target : room_players) {
			auto target_session = FindSession(target);
			if (!target_session) continue;
			target_session->SendPacket(send_buffer, send_data_size);
		}
	}

	for (auto& room_player : room_players) {
		auto session = FindSession(room_player);
		if (!session) continue;
		room_player.ClearSendBuffer();
	}
}

void TetrisRoom::Broadcast(char* packet)
{
	std::vector<int> target_index;
	const int packet_size = static_cast<int>(reinterpret_cast<const PACKET_HEADER*>(packet)->size);
	auto room_players = GetRoomPlayers();
	for (auto& room_player : room_players) {
		auto session = FindSession(room_player);
		if (!session) continue;
		session->SendPacket(packet, packet_size);
	}
}
