#include "TetrisTickData.h"

void TetrisTickCounters::InitTickData()
{
	left_tick_counter = 0;
	right_tick_counter = 0;
	down_tick_counter = 0;
	rotate_tick_counter = 0;
	drop_tick_counter = 0;
	down_timeout_tick_counter = 0; 
	garbage_line_tick_counter = 0;

	down_timeout = DOWN_TIMEOUT_TICK;
	garbage_line_timeout = GARBAGE_LINE_TIMEOUT_TICK;
}

void TetrisTickCounters::UpdateTickData()
{
	++left_tick_counter;
	++right_tick_counter;
	++down_tick_counter;
	++rotate_tick_counter;
	++drop_tick_counter;
	++down_timeout_tick_counter;
	++garbage_line_tick_counter;
}

