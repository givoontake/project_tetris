#include "RoomSession.h"

RoomSession::RoomSession()
{
	r_user_state.Store(ROOM_USER_STATE::EMPTY);
}

RoomSession::~RoomSession()
{

}

void RoomSession::InitRoomSession(Session& s)
{
	session = &s;
	session->StoreState(SESS_STATE::ROOM);
	tetris.Clear();
	r_user_state.Store(ROOM_USER_STATE::WAIT);
	prev_r_user_state.Store(ROOM_USER_STATE::WAIT);
	tetromino_index = 0;

	score = 0;
	ClearSendBuf();
}

void RoomSession::ClearRoomSession()
{
	session->StoreState(SESS_STATE::LOBBY);
	session = nullptr;
	tetris.Clear();
	r_user_state.Store(ROOM_USER_STATE::EMPTY);
	prev_r_user_state.Store(ROOM_USER_STATE::EMPTY);
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

void RoomSession::AddToSendBuffer(const char* data, int data_size)
{
	memcpy(send_buf + send_data_size, data, data_size);
	send_data_size += data_size;
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

