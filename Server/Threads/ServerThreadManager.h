#pragma once
#include <cstddef>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <vector>
#include <memory>
#include "ServerThread.h"

class IOCPServer;

class ServerThreadManager
{
public:
    static constexpr int MAX_TICK_WORKERS = 4;
    static constexpr int GAME_DB_WORKER_COUNT = 2;
    static constexpr int LOGIN_DB_WORKER_COUNT = 1;
    static constexpr int DB_WORKER_COUNT = GAME_DB_WORKER_COUNT + LOGIN_DB_WORKER_COUNT;

private:
    enum THREAD_GROUP { IO_THREADS, TICK_THREADS, TIMER_THREADS, DB_THREADS, THREAD_GROUP_COUNT };

    IOCPServer& iocp_server;
    std::mutex g_tick_mutex;
    std::condition_variable g_tick_cv;
    int g_tick_counter = 0;
    //bool tick_enable = false;

    std::vector<std::unique_ptr<ServerThread>> thread_objects;
    std::vector<std::vector<std::thread>> threads;
    std::mutex thread_mutex;
    bool closing = false;

    friend class TickThread;
    friend class TimerThread;

public:
    ServerThreadManager(IOCPServer& iocp_server);

    void StartThreads();
    void CloseThreads();
    void JoinThreads();
};
