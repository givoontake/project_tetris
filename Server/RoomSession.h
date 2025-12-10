#pragma once
#include <string>
#include "Tetris.h"
#include "define.h"
#include "Session.h"

class RoomSession
{
	Session* session = nullptr; // 상속으로 하면 세션을 받아올 수가 없음
	Tetris tetris;
	// std::string user_name; // 방 생성할 때 만들도록 일단 하고, 나중에 회원가입 - DB 연동으로 session 클래스에 포함해보자.
	// char user_name[MAX_USER_NAME];
	bool is_ready = false;
	Atomic<bool> in_use = false; // 룸에서 해당 배열 인덱스가 사용 중인지를 판별하기 위한 변수
	bool is_over = false;

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
	bool GetInUse() const { return in_use.GetSelf(); }
	bool GetIsReady() const { return is_ready; }
	Tetris& GetTetris() { return tetris; }
	int GetTetrominoIndex() const { return tetromino_index; }
	void AddTetrominoIndex() { ++tetromino_index; }
	bool GetIsOver() const { return is_over; }
	int GetDownTick() const { return down_tick_counter; }
	//int GetInputTick() const { return input_tick_counter; }
	int GetAddGarbageLineTick() const { return add_garbage_line_tick_counter; }
	int GetDownTimeout() const { return down_timeout_tick; }
	//int GetInputTimeout() const { return input_tick; }
	int GetAddGarbageLineTimeout() const { return add_garbage_line_tick; }
	int GetScore() const { return score; }

	void SetIsReady();
	bool SetUse(bool expected, bool desired);
	void SetIsOver(bool val) { is_over = val; }
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
	
};
