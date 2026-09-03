#pragma once
#include <chrono>
#include "enum_class.h"

class TetrisTimers
{
	using Clock = std::chrono::steady_clock;

	Clock::time_point left_input_available_time_;
	Clock::time_point right_input_available_time_;
	Clock::time_point down_input_available_time_;
	Clock::time_point rotate_input_available_time_;
	Clock::time_point drop_input_available_time_;
	Clock::time_point next_auto_down_time_;
	Clock::time_point next_garbage_line_time_;

public:
	TetrisTimers();

	void Reset();
	bool IsInputAllowed(EventType move_type, Clock::time_point tick_time) const;
	void RecordInput(EventType move_type, Clock::time_point tick_time);
	bool IsAutoDownDue(Clock::time_point tick_time) const;
	void RestartAutoDown(Clock::time_point tick_time);
	bool IsGarbageLineDue(Clock::time_point tick_time) const;
	void RestartGarbageLine(Clock::time_point tick_time);
};
