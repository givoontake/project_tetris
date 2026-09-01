#include "TetrisTickData.h"

void TetrisTickCounters::InitTickData()
{
	left_tick_counter_ = 0;
	right_tick_counter_ = 0;
	down_tick_counter_ = 0;
	rotate_tick_counter_ = 0;
	drop_tick_counter_ = 0;
	down_timeout_tick_counter_ = 0;
	garbage_line_tick_counter_ = 0;

	down_timeout_ = DOWN_TIMEOUT_TICK;
	garbage_line_timeout_ = GARBAGE_LINE_TIMEOUT_TICK;
}

void TetrisTickCounters::UpdateTickData()
{
	++left_tick_counter_;
	++right_tick_counter_;
	++down_tick_counter_;
	++rotate_tick_counter_;
	++drop_tick_counter_;
	++down_timeout_tick_counter_;
	++garbage_line_tick_counter_;
}

