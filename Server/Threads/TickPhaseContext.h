#pragma once
#include <atomic>

struct TickPhaseContext
{
    std::atomic<int> next_room_index{ 0 };
    const long long tick_time_ms;

    TickPhaseContext(long long tick_time_ms);
};
