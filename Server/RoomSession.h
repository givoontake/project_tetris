#pragma once
#include <string>
#include <array>
#include "Tetris.h"
#include "define.h"
#include "Session.h"

enum class ROOM_USER_STATE { EMPTY, WAIT, READY, PLAY, GAMEOVER };

class RoomSession
{
	Session* session = nullptr; // 상속으로 하면 세션을 받아올 수가 없음
	Tetris tetris;
	char send_buf[BUF_SIZE];
	int send_data_size = 0;
	// std::string user_name; // 방 생성할 때 만들도록 일단 하고, 나중에 회원가입 - DB 연동으로 session 클래스에 포함해보자.
	// char user_name[MAX_USER_NAME];
	Atomic<ROOM_USER_STATE> r_user_state;

	int tetromino_index = 0;
	int down_tick_counter = 0;
	//int input_tick_counter = 0;
	int add_garbage_line_tick_counter = 0;

	int down_timeout_tick = MOVE_DOWN_TIMEOUT_TICK;
	//int input_tick = INPUT_TICK;
	int add_garbage_line_tick = ADD_GARBAGE_LINE_TICK;

	int score = 0;

public:
	RoomSession();
	~RoomSession();

	Session* GetSession() const { return session; }
	ROOM_USER_STATE GetRoomUserState() const { return r_user_state.Load(); }
	Tetris& GetTetris() { return tetris; }
	int GetTetrominoIndex() const { return tetromino_index; }
	void AddTetrominoIndex() { ++tetromino_index; }
	int GetDownTick() const { return down_tick_counter; }
	//int GetInputTick() const { return input_tick_counter; }
	int GetAddGarbageLineTick() const { return add_garbage_line_tick_counter; }
	int GetDownTimeout() const { return down_timeout_tick; }
	//int GetInputTimeout() const { return input_tick; }
	int GetAddGarbageLineTimeout() const { return add_garbage_line_tick; }
	int GetScore() const { return score; }

	void SetRoomUserState(ROOM_USER_STATE new_state) { return r_user_state.Store(new_state); }
	void SetDownTick(int val) { down_tick_counter = val; }
	//void SetInputTick(int val) { input_tick_counter = val; }
	void SetAddGarbageLineTick(int val) { add_garbage_line_tick_counter = val; }
	void SetDownTimeout(int val) { down_timeout_tick = val; }
	//void SetInputTick(int val) { input_tick = val; }
	void SetAddGarbageLineTimeout(int val) { add_garbage_line_tick = val; }
	void SetScore(int val) { score = val; }	

	void InitSession(Session* s);
	void ClearSession();
	void ClearData();
	void AddToSendBuffer(const char* data, int data_size);
	void SendTickBatch(HANDLE iocp_handle);
	void ClearSendBuf();
};
