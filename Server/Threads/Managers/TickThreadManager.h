#pragma once
#include <condition_variable>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>
#include "TickThread.h"
#include "tick_state.h"

class IOCPServer;
struct TickPhaseContext;
class TimerThread;

class TickThreadManager
{
public:
    static constexpr int THREAD_COUNT = 12;
    static constexpr int MIN_AVAILABLE_THREAD_COUNT = THREAD_COUNT / 2;

private:
    IOCPServer& iocp_server_;
    std::mutex tick_mutex_;
    std::condition_variable tick_cv_;
    // 혼합 대기의 사전 기상에 쓰던 공유 시각이며, 현재는 타이머 내부에서만 다음 틱 시각을 관리한다.
    // std::atomic<std::chrono::steady_clock::duration::rep> next_tick_time_count_{ 0 };
    TickWaitPolicy tick_wait_policy_;
    //bool tick_enable = false;
    std::vector<std::unique_ptr<TickThread>> thread_objects_;
    std::vector<std::thread> threads_;

    friend class TickThread;
    friend class TimerThread;

    int GetAvailableThreadCount() const;
    std::vector<TickThread*> SelectThreads();

public:
    TickThreadManager(IOCPServer& iocp_server, TickWaitPolicy tick_policy);

    void Start();
    bool StartTickPhase(long long tick_time_ms);
    void Close();
    void Join();
};
