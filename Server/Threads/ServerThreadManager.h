#pragma once
#include <chrono>
#include <cstddef>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <vector>
#include <memory>
#include "ServerThread.h"
#include "tick_state.h"

class IOCPServer;
class TickThread;
class TickPhaseContext;

class ServerThreadManager
{
public:
    static constexpr int MAX_TICK_THREADS = 12;
    static constexpr int MIN_AVAILABLE_TICK_THREADS = MAX_TICK_THREADS / 2;
    // static constexpr int TICK_SPIN_WAIT_MARGIN_MS = 5;
    static constexpr int GAME_DB_THREAD_COUNT = 2;
    static constexpr int LOGIN_DB_THREAD_COUNT = 1;
    static constexpr int DB_THREAD_COUNT = GAME_DB_THREAD_COUNT + LOGIN_DB_THREAD_COUNT;

private:
    enum ThreadGroup { IO_THREADS, TICK_THREADS, TIMER_THREADS, DB_THREADS, THREAD_GROUP_COUNT };

    IOCPServer& iocp_server_;
    std::mutex tick_mutex_;
    std::condition_variable tick_cv_;
    // 혼합 대기의 사전 기상에 쓰던 공유 시각이며, 현재는 타이머 내부에서만 다음 틱 시각을 관리한다.
    // std::atomic<std::chrono::steady_clock::duration::rep> next_tick_time_count_{ 0 };
    TickWaitPolicy tick_wait_policy_;
    //bool tick_enable = false;

    std::vector<std::unique_ptr<ServerThread>> thread_objects_;
    std::vector<TickThread*> tick_threads_;
    std::vector<std::vector<std::thread>> threads_;
    std::mutex thread_mutex_;
    bool is_closing_ = false;

    friend class TickThread;
    friend class TimerThread;

    int GetAvailableTickThreadCount() const;
    std::vector<TickThread*> SelectTickThreads();
    void CompleteTickThreadPhase(TickThread& tick_thread, const std::shared_ptr<TickPhaseContext>& phase_context);

public:
    ServerThreadManager(IOCPServer& iocp_server, TickWaitPolicy tick_policy = TickWaitPolicy::FULL_SPIN);

    void StartThreads();
    bool StartTickPhase(std::chrono::steady_clock::time_point tick_time);
    void CloseThreads();
    void JoinThreads();
};
