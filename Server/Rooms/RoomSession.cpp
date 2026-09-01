#include "RoomSession.h"

RoomSession::RoomSession()
{
	r_user_state_.store(RoomUserState::WAIT);
	prev_r_user_state_.store(RoomUserState::WAIT);
}

RoomSession::~RoomSession()
{

}

bool RoomSession::InitRoomSession(const SP<Session>& s, int room_index)
{
	if (!s) return false;
	if (s->GetLifeState() != LifeState::ACTIVE) return false;
	const RoomSnapShot room_snapshot = s->GetRoomSnapShot();
	if (room_snapshot.state == ModeState::LOBBY) {
		if (!s->TrySetRoomMode(room_index)) return false;
	}
	else if (room_snapshot.state != ModeState::ROOM || room_snapshot.room_index != room_index) return false;
	session_ = s;
	tetris_.Clear();
	r_user_state_.store(RoomUserState::WAIT);
	prev_r_user_state_.store(RoomUserState::WAIT);
	tetromino_index_ = 0;

	score_ = 0;
	ClearSendBuf();
	return true;
}

void RoomSession::ClearRoomSession()
{
	if (auto session_ptr = session_.lock()) session_ptr->SetRoomSnapShot(ModeState::LOBBY, -1);
	session_.reset();
	tetris_.Clear();
	r_user_state_.store(RoomUserState::WAIT);
	prev_r_user_state_.store(RoomUserState::WAIT);
	tetromino_index_ = 0;

	score_ = 0;
	ClearSendBuf();
}

void RoomSession::ClearData()
{
	tetris_.Clear();
	r_user_state_.store(RoomUserState::WAIT);
	prev_r_user_state_.store(RoomUserState::WAIT);
	tetromino_index_ = 0;

	score_ = 0;
	ClearSendBuf();
}

bool RoomSession::AddToSendBuffer(const char* data, int data_size)
{
	if (data_size <= 0) return true;
	const int need_size = send_data_size_ + data_size;
	if (need_size > BUF_SIZE) return false;
	memcpy(send_buf_ + send_data_size_, data, data_size);
	send_data_size_ += data_size;
	return true;
}

//void RoomSession::SendTickData(HANDLE iocp_handle)
//{
//	if (send_data_size_ >= 3) {
//		session_->SendBoundPacket(send_buf_, send_data_size_, iocp_handle);
//	}
//}

void RoomSession::ClearSendBuf()
{
	send_data_size_ = 0;
	ZeroMemory(send_buf_, sizeof(send_buf_));
}

