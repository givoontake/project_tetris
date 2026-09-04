#pragma once
#include <atomic>
#include <cstddef>

struct LobbyPhaseContext
{
	std::atomic<bool> is_lifecycle_claimed{ false };
	std::atomic<bool> is_lifecycle_complete{ false };
	std::atomic<int> next_session_index{ 0 };
	const std::size_t lifecycle_task_count;

	explicit LobbyPhaseContext(std::size_t lifecycle_task_count);
};
