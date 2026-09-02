#pragma once
#include "define_packets.h"
#include "settings.h"

class TetrisTickCounters
{
	// 인풋
	int left_tick_counter_ = 0;
	int right_tick_counter_ = 0;
	int down_tick_counter_ = 0;
	int rotate_tick_counter_ = 0;
	int drop_tick_counter_ = 0;
	// 자동 하강
	int down_timeout_tick_counter_ = 0;
	int garbage_line_tick_counter_ = 0;
	// 타임아웃 틱
	int down_timeout_ = DOWN_TIMEOUT_TICK;
	int garbage_line_timeout_ = GARBAGE_LINE_TIMEOUT_TICK;

public:
	//getters
	int GetLeftTick() const { return left_tick_counter_; }
	int GetRightTick() const { return right_tick_counter_; }
	int GetDownTick() const { return down_tick_counter_; }
	int GetRotateTick() const { return rotate_tick_counter_; }
	int GetDropTick() const { return drop_tick_counter_; }
	int GetDownTimeoutTick() const { return down_timeout_tick_counter_; }
	int GetGarbageLineTick() const { return garbage_line_tick_counter_; }

	int GetDownTimeout() const { return down_timeout_; }
	int GetGarbageLineTimeout() const { return garbage_line_timeout_; }

	//setters
	void SetLeftTick(int val) { left_tick_counter_ = val; }
	void SetRightTick(int val) { right_tick_counter_ = val; }
	void SetDownTick(int val) { down_tick_counter_ = val; }
	void SetRotateTick(int val) { rotate_tick_counter_ = val; }
	void SetDropTick(int val) { drop_tick_counter_ = val; }
	void SetDownTimeoutTick(int val) { down_timeout_tick_counter_ = val; }
	void SetGarbageLineTick(int val) { garbage_line_tick_counter_ = val; }


	void InitTickData();
	void UpdateTickData();
};
