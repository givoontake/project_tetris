#include "TetrisTimers.h"
#include "settings.h"

TetrisTimers::TetrisTimers()
{
	Reset();
}

void TetrisTimers::Reset()
{
	const long long current_time_ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
	left_input_available_time_ms_ = current_time_ms;
	right_input_available_time_ms_ = current_time_ms;
	down_input_available_time_ms_ = current_time_ms;
	rotate_input_available_time_ms_ = current_time_ms;
	drop_input_available_time_ms_ = current_time_ms;
	next_auto_down_time_ms_ = current_time_ms + DOWN_TIMEOUT_MS;
	next_garbage_line_time_ms_ = current_time_ms + GARBAGE_LINE_TIMEOUT_MS;
}

bool TetrisTimers::IsInputAllowed(EventType move_type, long long tick_time_ms) const
{
	switch (move_type) {
	case EventType::LEFT:
		return tick_time_ms >= left_input_available_time_ms_;
	case EventType::RIGHT:
		return tick_time_ms >= right_input_available_time_ms_;
	case EventType::DOWN:
		return tick_time_ms >= down_input_available_time_ms_;
	case EventType::ROTATE:
		return tick_time_ms >= rotate_input_available_time_ms_;
	case EventType::DROP:
		return tick_time_ms >= drop_input_available_time_ms_;
	default:
		return false;
	}
}

void TetrisTimers::RecordInput(EventType move_type, long long tick_time_ms)
{
	switch (move_type) {
	case EventType::LEFT:
		left_input_available_time_ms_ = tick_time_ms + MOVE_TIMEOUT_MS;
		break;
	case EventType::RIGHT:
		right_input_available_time_ms_ = tick_time_ms + MOVE_TIMEOUT_MS;
		break;
	case EventType::DOWN:
		down_input_available_time_ms_ = tick_time_ms + MOVE_TIMEOUT_MS;
		break;
	case EventType::ROTATE:
		rotate_input_available_time_ms_ = tick_time_ms + ROTATE_TIMEOUT_MS;
		break;
	case EventType::DROP:
		drop_input_available_time_ms_ = tick_time_ms + DROP_TIMEOUT_MS;
		break;
	default:
		break;
	}
}

bool TetrisTimers::IsAutoDownDue(long long tick_time_ms) const
{
	return tick_time_ms >= next_auto_down_time_ms_;
}

void TetrisTimers::RestartAutoDown(long long tick_time_ms)
{
	next_auto_down_time_ms_ = tick_time_ms + DOWN_TIMEOUT_MS;
}

bool TetrisTimers::IsGarbageLineDue(long long tick_time_ms) const
{
	return tick_time_ms >= next_garbage_line_time_ms_;
}

void TetrisTimers::RestartGarbageLine(long long tick_time_ms)
{
	next_garbage_line_time_ms_ = tick_time_ms + GARBAGE_LINE_TIMEOUT_MS;
}
