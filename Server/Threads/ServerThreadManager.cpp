#include <thread>
#include "ServerThreadManager.h"
#include "../IOCPServer.h"
#include "IOThread.h"
#include "TickThread.h"
#include "TimerThread.h"

ServerThreadManager::ServerThreadManager(IOCPServer& iocp_server)
    : iocp_server(iocp_server), threads(THREAD_GROUP_COUNT)
{
}

void ServerThreadManager::StartThreads()
{
    std::lock_guard<std::mutex> lock(thread_mutex);
    int num_threads = std::thread::hardware_concurrency();

    for (int i = 0; i < MAX_TICK_WORKERS; ++i){
        thread_objects.emplace_back(std::make_unique<TickThread>(*this, i));
        thread_objects.back()->Start();
        threads[TICK_THREADS].emplace_back(&ServerThread::Run, thread_objects.back().get());
        --num_threads;
    }

    thread_objects.emplace_back(std::make_unique<TimerThread>(*this));
    thread_objects.back()->Start();
    threads[TIMER_THREADS].emplace_back(&ServerThread::Run, thread_objects.back().get());
    --num_threads;

    iocp_server.StartDBWorkers();
    for (int i = 0; i < GAME_DB_WORKER_COUNT; ++i) {
        threads[DB_THREADS].emplace_back(&ServerThread::Run, &iocp_server.game_db_workers[i]);
    }
    for (int i = 0; i < LOGIN_DB_WORKER_COUNT; ++i) {
        threads[DB_THREADS].emplace_back(&ServerThread::Run, &iocp_server.login_db_worker);
    }
    num_threads -= DB_WORKER_COUNT;
    iocp_server.RequestLoadRanking();

    for (int i = 0; i < num_threads; ++i) {
        thread_objects.emplace_back(std::make_unique<IOThread>(iocp_server));
        thread_objects.back()->Start();
        threads[IO_THREADS].emplace_back(&ServerThread::Run, thread_objects.back().get());
    }
}

void ServerThreadManager::CloseThreads()
{
    std::lock_guard<std::mutex> lock(thread_mutex);
    if (closing) return;
    closing = true;
    iocp_server.is_running = false;

    for (auto& thread_object : thread_objects)
        thread_object->Close();
}

void ServerThreadManager::JoinThreads()
{
    for (std::size_t group_index = 0; group_index < threads.size(); ++group_index) {
        if (group_index == TICK_THREADS) CloseThreads();
        if (group_index == DB_THREADS) iocp_server.StopDBWorkers();
        for (auto& th : threads[group_index])
            th.join();
    }
}

