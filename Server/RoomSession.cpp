#include "RoomSession.h"

RoomSession::RoomSession()
{
	r_user_state.Store(ROOM_USER_STATE::EMPTY);
}

RoomSession::~RoomSession()
{

}

void RoomSession::InitSession(Session* s)
{
	session = s;
	tetris.Clear();
	r_user_state.Store(ROOM_USER_STATE::WAIT);
	tetromino_index = 0;

	down_tick_counter = 0;
	//input_tick_counter = 0;
	add_garbage_line_tick_counter = 0;

	down_timeout_tick = MOVE_DOWN_TIMEOUT_TICK;
	//input_tick = INPUT_TICK;
	add_garbage_line_tick = ADD_GARBAGE_LINE_TICK;

	score = 0;
	ClearSendBuf();
}

void RoomSession::ClearSession()
{
	session = nullptr;
	tetris.Clear();
	r_user_state.Store(ROOM_USER_STATE::EMPTY);
	tetromino_index = 0;

	down_tick_counter = 0;
	//input_tick_counter = 0;
	add_garbage_line_tick_counter = 0;

	down_timeout_tick = MOVE_DOWN_TIMEOUT_TICK;
	//input_tick = INPUT_TICK;
	add_garbage_line_tick = ADD_GARBAGE_LINE_TICK;

	score = 0;
	ClearSendBuf();
}

void RoomSession::ClearData()
{
	tetris.Clear();
	r_user_state.Store(ROOM_USER_STATE::WAIT);
	tetromino_index = 0;

	down_tick_counter = 0;
	//input_tick_counter = 0;
	add_garbage_line_tick_counter = 0;

	down_timeout_tick = MOVE_DOWN_TIMEOUT_TICK;
	//input_tick = INPUT_TICK;
	add_garbage_line_tick = ADD_GARBAGE_LINE_TICK;

	score = 0;
	ClearSendBuf();
}

void RoomSession::AddToSendBuffer(const char* data, int data_size)
{
	memcpy(send_buf + send_data_size, data, data_size);
	send_data_size += data_size;
}

void RoomSession::SendTickBatch(HANDLE iocp_handle)
{
	if (send_data_size >= 3) {
		session->SendBoundPacket(send_buf, send_data_size, iocp_handle);
		ClearSendBuf();
	}
	
}

void RoomSession::ClearSendBuf()
{
	send_data_size = 0;
	ZeroMemory(send_buf, sizeof(send_buf));
}

