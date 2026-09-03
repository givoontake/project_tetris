#include "TickPhaseContext.h"

TickPhaseContext::TickPhaseContext(int thread_count, std::chrono::steady_clock::time_point tick_time)
    : THREAD_COUNT(thread_count), TICK_TIME(tick_time)
{
}
