#pragma once
#include <chrono>
#include "enum_class.h"

class TetrisTimers
{
	long long left_input_available_time_ms_;
	long long right_input_available_time_ms_;
	long long down_input_available_time_ms_;
	long long rotate_input_available_time_ms_;
	long long drop_input_available_time_ms_;
	long long next_auto_down_time_ms_;
	long long next_garbage_line_time_ms_;

public:
	TetrisTimers();

	void Reset();
	bool IsInputAllowed(EventType move_type, long long tick_time_ms) const;
	void RecordInput(EventType move_type, long long tick_time_ms);
	bool IsAutoDownDue(long long tick_time_ms) const;
	void RestartAutoDown(long long tick_time_ms);
	bool IsGarbageLineDue(long long tick_time_ms) const;
	void RestartGarbageLine(long long tick_time_ms);
};
