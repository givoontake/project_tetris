#include "Player.h"

Player::Player()
{
	room_player_state_.store(RoomPlayerState::WAIT);
	prev_room_player_state_.store(RoomPlayerState::WAIT);
}

Player::~Player()
{

}

bool Player::InitPlayer(const SP<Session>& session, int room_index)
{
	if (!session) return false;
	if (session->GetLifeState() != LifeState::ACTIVE) return false;
	const RoomSnapshot room_snapshot = session->GetRoomSnapshot();
	if (room_snapshot.mode_state == ModeState::LOBBY) {
		if (!session->TrySetRoomMode(room_index)) return false;
	}
	else if (room_snapshot.mode_state != ModeState::ROOM || room_snapshot.room_index != room_index) return false;
	session_ = session;
	tetris_.Clear();
	room_player_state_.store(RoomPlayerState::WAIT);
	prev_room_player_state_.store(RoomPlayerState::WAIT);
	tetromino_index_ = 0;

	score_ = 0;
	ClearSendBuffer();
	return true;
}

void Player::ClearPlayer()
{
	if (auto session_ptr = session_.lock()) session_ptr->SetRoomSnapshot(ModeState::LOBBY, -1);
	session_.reset();
	tetris_.Clear();
	room_player_state_.store(RoomPlayerState::WAIT);
	prev_room_player_state_.store(RoomPlayerState::WAIT);
	tetromino_index_ = 0;

	score_ = 0;
	ClearSendBuffer();
}

void Player::ResetGameData()
{
	tetris_.Clear();
	room_player_state_.store(RoomPlayerState::WAIT);
	prev_room_player_state_.store(RoomPlayerState::WAIT);
	tetromino_index_ = 0;

	score_ = 0;
	ClearSendBuffer();
}

bool Player::AddToSendBuffer(const char* data, int data_size)
{
	if (data_size <= 0) return true;
	const int need_size = send_data_size_ + data_size;
	if (need_size > BUF_SIZE) return false;
	memcpy(send_buffer_ + send_data_size_, data, data_size);
	send_data_size_ += data_size;
	return true;
}

//void Player::SendTickData(HANDLE iocp_handle)
//{
//	if (send_data_size_ >= 3) {
//		session_->SendBoundPacket(send_buffer_, send_data_size_, iocp_handle);
//	}
//}

void Player::ClearSendBuffer()
{
	send_data_size_ = 0;
	ZeroMemory(send_buffer_, sizeof(send_buffer_));
}

