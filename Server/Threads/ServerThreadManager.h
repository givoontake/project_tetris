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
    static constexpr int MAX_TICK_WORKERS = 12;
    static constexpr int MIN_AVAILABLE_TICK_WORKERS = MAX_TICK_WORKERS / 2;
    // static constexpr int TICK_SPIN_WAIT_MARGIN_MS = 5;
    static constexpr int GAME_DB_WORKER_COUNT = 2;
    static constexpr int LOGIN_DB_WORKER_COUNT = 1;
    static constexpr int DB_WORKER_COUNT = GAME_DB_WORKER_COUNT + LOGIN_DB_WORKER_COUNT;

private:
    enum THREAD_GROUP { IO_THREADS, TICK_THREADS, TIMER_THREADS, DB_THREADS, THREAD_GROUP_COUNT };

    IOCPServer& iocp_server;
    std::mutex g_tick_mutex;
    std::condition_variable g_tick_cv;
    // 혼합 대기의 사전 기상에 쓰던 공유 시각이며, 현재는 타이머 내부에서만 다음 틱 시각을 관리한다.
    // std::atomic<std::chrono::steady_clock::duration::rep> next_tick_time_count{ 0 };
    TICK_WAIT_POLICY tick_wait_policy;
    //bool tick_enable = false;

    std::vector<std::unique_ptr<ServerThread>> thread_objects;
    std::vector<TickThread*> tick_threads;
    std::vector<std::vector<std::thread>> threads;
    std::mutex thread_mutex;
    bool closing = false;

    friend class TickThread;
    friend class TimerThread;

    int GetCompleteTickWorkerCount() const;
    std::vector<TickThread*> SelectTickWorkers();
    void CompleteTickPhase(TickThread& tick_thread, const std::shared_ptr<TickPhaseContext>& phase_context);

public:
    ServerThreadManager(IOCPServer& iocp_server, TICK_WAIT_POLICY tick_policy = TICK_WAIT_POLICY::FULL_SPIN);

    void StartThreads();
    bool StartTickPhase();
    void CloseThreads();
    void JoinThreads();
};
