#include "TetrisTimers.h"
#include "settings.h"

TetrisTimers::TetrisTimers()
{
	Reset();
}

void TetrisTimers::Reset()
{
	const auto current_time = Clock::now();
	left_input_available_time_ = current_time;
	right_input_available_time_ = current_time;
	down_input_available_time_ = current_time;
	rotate_input_available_time_ = current_time;
	drop_input_available_time_ = current_time;
	next_auto_down_time_ = current_time + std::chrono::milliseconds(DOWN_TIMEOUT_MS);
	next_garbage_line_time_ = current_time + std::chrono::milliseconds(GARBAGE_LINE_TIMEOUT_MS);
}

bool TetrisTimers::IsInputAllowed(EventType move_type, Clock::time_point tick_time) const
{
	switch (move_type) {
	case EventType::LEFT:
		return tick_time >= left_input_available_time_;
	case EventType::RIGHT:
		return tick_time >= right_input_available_time_;
	case EventType::DOWN:
		return tick_time >= down_input_available_time_;
	case EventType::ROTATE:
		return tick_time >= rotate_input_available_time_;
	case EventType::DROP:
		return tick_time >= drop_input_available_time_;
	default:
		return false;
	}
}

void TetrisTimers::RecordInput(EventType move_type, Clock::time_point tick_time)
{
	switch (move_type) {
	case EventType::LEFT:
		left_input_available_time_ = tick_time + std::chrono::milliseconds(MOVE_TIMEOUT_MS);
		break;
	case EventType::RIGHT:
		right_input_available_time_ = tick_time + std::chrono::milliseconds(MOVE_TIMEOUT_MS);
		break;
	case EventType::DOWN:
		down_input_available_time_ = tick_time + std::chrono::milliseconds(MOVE_TIMEOUT_MS);
		break;
	case EventType::ROTATE:
		rotate_input_available_time_ = tick_time + std::chrono::milliseconds(ROTATE_TIMEOUT_MS);
		break;
	case EventType::DROP:
		drop_input_available_time_ = tick_time + std::chrono::milliseconds(DROP_TIMEOUT_MS);
		break;
	default:
		break;
	}
}

bool TetrisTimers::IsAutoDownDue(Clock::time_point tick_time) const
{
	return tick_time >= next_auto_down_time_;
}

void TetrisTimers::RestartAutoDown(Clock::time_point tick_time)
{
	next_auto_down_time_ = tick_time + std::chrono::milliseconds(DOWN_TIMEOUT_MS);
}

bool TetrisTimers::IsGarbageLineDue(Clock::time_point tick_time) const
{
	return tick_time >= next_garbage_line_time_;
}

void TetrisTimers::RestartGarbageLine(Clock::time_point tick_time)
{
	next_garbage_line_time_ = tick_time + std::chrono::milliseconds(GARBAGE_LINE_TIMEOUT_MS);
}
