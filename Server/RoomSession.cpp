#include "RoomSession.h"

RoomSession::RoomSession(int max_user)
	: tick_buf{},
	send_buf(std::make_unique<char[]>(BUF_SIZE * (max_user > 0 ? max_user : 1))),
	send_buf_size(BUF_SIZE * (max_user > 0 ? max_user : 1)),
	tick_data_size(0),
	send_data_size(0),
	r_user_state(ROOM_USER_STATE::WAIT),
	prev_r_user_state(ROOM_USER_STATE::WAIT),
	tetromino_index(0),
	score(0),
	combo(0)
{
	ZeroMemory(send_buf.get(), send_buf_size);
}

bool RoomSession::InitRoomSession(const SP<Session>& s, int room_index)
{
	if (!s) return false;
	if (!s->TrySetRoomMode(room_index)) return false;
	session = s;
	tetris.Clear();
	r_user_state.Store(ROOM_USER_STATE::WAIT);
	prev_r_user_state.Store(ROOM_USER_STATE::WAIT);
	tetromino_index = 0;

	score = 0;
	combo = 0;
	ClearBuffers();
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
	combo = 0;
	ClearBuffers();
}

void RoomSession::ClearData()
{
	tetris.Clear();
	r_user_state.Store(ROOM_USER_STATE::WAIT);
	prev_r_user_state.Store(ROOM_USER_STATE::WAIT);
	tetromino_index = 0;

	score = 0;
	combo = 0;
	ClearBuffers();
}

bool RoomSession::AddToTickBuffer(const char* data, int data_size)
{
	if (data_size <= 0) return true;
	const int need_size = tick_data_size + data_size;
	if (need_size > BUF_SIZE) return false;
	memcpy(tick_buf + tick_data_size, data, data_size);
	tick_data_size += data_size;
	return true;
}

bool RoomSession::AddToSendBuffer(const char* data, int data_size)
{
	if (data_size <= 0) return true;
	const int need_size = send_data_size + data_size;
	if (!send_buf || need_size > send_buf_size) return false;
	memcpy(send_buf.get() + send_data_size, data, data_size);
	send_data_size += data_size;
	return true;
}

//void RoomSession::SendTickData(HANDLE iocp_handle)
//{
//	if (send_data_size >= 3) {
//		session->SendBoundPacket(send_buf, send_data_size, iocp_handle);
//	}
//}

void RoomSession::ClearTickBuf()
{
	tick_data_size = 0;
	ZeroMemory(tick_buf, sizeof(tick_buf));
}

void RoomSession::ClearSendBuf()
{
	send_data_size = 0;
	if (send_buf && send_buf_size > 0) ZeroMemory(send_buf.get(), send_buf_size);
}

void RoomSession::ClearBuffers()
{
	ClearTickBuf();
	ClearSendBuf();
}
