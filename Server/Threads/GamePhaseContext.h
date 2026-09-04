#pragma once
#include <atomic>
#include <cstddef>

struct GamePhaseContext
{
	std::atomic<bool> is_lifecycle_claimed{ false };
	std::atomic<bool> is_lifecycle_complete{ false };
    std::atomic<int> next_room_index{ 0 };
    const long long tick_time_ms;

	explicit GamePhaseContext(long long tick_time_ms);
};
