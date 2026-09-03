#pragma once
#include <atomic>
#include <chrono>

class TickPhaseContext
{
public:
    std::atomic<int> next_room_index_{ 0 };
    const int THREAD_COUNT;
    const std::chrono::steady_clock::time_point TICK_TIME;
    std::atomic<int> completed_thread_count_{ 0 };

    TickPhaseContext(int thread_count, std::chrono::steady_clock::time_point tick_time);
};
