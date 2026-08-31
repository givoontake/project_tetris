#pragma once
#include <atomic>

class TickPhaseContext
{
public:
    std::atomic<int> room_index_counter{ 0 };
    const int worker_count;
    std::atomic<int> completed_worker_count{ 0 };

    explicit TickPhaseContext(int worker_count);
};
