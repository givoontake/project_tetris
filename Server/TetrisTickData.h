#pragma once
#include "define.h"
#include "settings.h"

class TetrisTickCounters
{
	// 인풋
	int left_tick_counter = 0;
	int right_tick_counter = 0;
	int down_tick_counter = 0; 
	int rotate_tick_counter = 0;
	int drop_tick_counter = 0;
	// 자동 하강
	int down_timeout_tick_counter = 0;
	int garbage_line_tick_counter = 0;
	// 타임아웃 틱
	int down_timeout = DOWN_TIMEOUT_TICK;
	int garbage_line_timeout = GARBAGE_LINE_TIMEOUT_TICK;

public:
	//getters
	int GetLeftTick() const { return left_tick_counter; }
	int GetRightTick() const { return right_tick_counter; }
	int GetDownTick() const { return down_tick_counter; }
	int GetRotateTick() const { return rotate_tick_counter; }
	int GetDropTick() const { return drop_tick_counter; }
	int GetDownTimeoutTick() const { return down_timeout_tick_counter; }
	int GetGarbageLineTick() const { return garbage_line_tick_counter; }

	int GetDownTimeout() const { return down_timeout; }
	int GetGarbageLineTimeout() const { return garbage_line_timeout; }

	//setters
	void SetLeftTick(int val) { left_tick_counter = val; }
	void SetRightTick(int val) { right_tick_counter = val; }
	void SetDownTick(int val) { down_tick_counter = val; }
	void SetRotateTick(int val) { rotate_tick_counter = val; }
	void SetDropTick(int val) { drop_tick_counter = val; }
	void SetDownTimeoutTick(int val) { down_timeout_tick_counter = val; }
	void SetGarbageLineTick(int val) { garbage_line_tick_counter = val; }

	void SetDownTimeout(int val) { down_timeout = val; }
	void SetGarbageLineTimeout(int val) { garbage_line_timeout = val; }

	void InitTickData();
	void UpdateTickData();
};