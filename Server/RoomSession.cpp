#include "RoomSession.h"

RoomSession::RoomSession()
{
	r_user_state.Store(ROOM_USER_STATE::WAIT);
	prev_r_user_state.Store(ROOM_USER_STATE::WAIT);
}

RoomSession::~RoomSession()
{

}

bool RoomSession::InitRoomSession(const SP<Session>& s, int room_index)
{
	if (!s) return false;
	if (s->GetLifeState() != LIFE_STATE::ACTIVE) return false;
	const RoomSnapShot room_snapshot = s->GetRoomSnapShot();
	if (room_snapshot.state == MODE_STATE::LOBBY) {
		if (!s->TrySetRoomMode(room_index)) return false;
	}
	else if (room_snapshot.state != MODE_STATE::ROOM || room_snapshot.room_index != room_index) return false;
	session = s;
	tetris.Clear();
	r_user_state.Store(ROOM_USER_STATE::WAIT);
	prev_r_user_state.Store(ROOM_USER_STATE::WAIT);
	tetromino_index = 0;

	score = 0;
	ClearSendBuf();
	return true;
}

void RoomSession::ClearRoomSession()
{
	if (auto session_ptr = session.lock()) session_ptr->SetRoomSnapShot(MODE_STATE::LOBBY, -1);
	session.reset();
	tetris.Clear();
	r_user_state.Store(ROOM_USER_STATE::WAIT);
	prev_r_user_state.Store(ROOM_USER_STATE::WAIT);
	tetromino_index = 0;

	score = 0;
	ClearSendBuf();
}

void RoomSession::ClearData()
{
	tetris.Clear();
	r_user_state.Store(ROOM_USER_STATE::WAIT);
	prev_r_user_state.Store(ROOM_USER_STATE::WAIT);
	tetromino_index = 0;

	score = 0;
	ClearSendBuf();
}

bool RoomSession::AddToSendBuffer(const char* data, int data_size)
{
	if (data_size <= 0) return true;
	const int need_size = send_data_size + data_size;
	if (need_size > BUF_SIZE) return false;
	memcpy(send_buf + send_data_size, data, data_size);
	send_data_size += data_size;
	return true;
}

//void RoomSession::SendTickData(HANDLE iocp_handle)
//{
//	if (send_data_size >= 3) {
//		session->SendBoundPacket(send_buf, send_data_size, iocp_handle);
//	}
//}

void RoomSession::ClearSendBuf()
{
	send_data_size = 0;
	ZeroMemory(send_buf, sizeof(send_buf));
}

