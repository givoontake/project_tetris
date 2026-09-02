#pragma once
#include <atomic>

class TickPhaseContext
{
public:
    std::atomic<int> next_room_index_{ 0 };
    const int THREAD_COUNT;
    std::atomic<int> completed_thread_count_{ 0 };

    explicit TickPhaseContext(int thread_count);
};
