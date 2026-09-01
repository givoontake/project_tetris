#pragma once
#include <atomic>

class TickPhaseContext
{
public:
    std::atomic<int> room_index_counter_{ 0 };
    const int WORKER_COUNT;
    std::atomic<int> completed_worker_count_{ 0 };

    explicit TickPhaseContext(int worker_count);
};
