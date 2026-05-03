#include <iostream>
#include <vector>
#include <thread>
#include <memory>
#include "IOCPServer.h"
#include "ServerThreadManager.h"

int main()
{
    SetConsoleOutputCP(65001);
    auto iocp_server = std::make_unique<IOCPServer>();

    iocp_server->StartServer();
    iocp_server->InitStressTestRooms();
    iocp_server->GetDB().init(iocp_server->GetHandle(), iocp_server->IsStressTestMode());
    ServerThreadManager server_thread_manager(*iocp_server);

    std::vector<std::thread> worker_threads;
    int num_threads = std::thread::hardware_concurrency();
    if (num_threads <= 0) num_threads = 1;
    int io_worker_count = num_threads - MAX_TICK_WORKERS - 3;
    if (io_worker_count < 1) io_worker_count = 1;

    std::vector<std::thread> tick_workers;
    for (int i = 0; i < MAX_TICK_WORKERS; ++i){
        tick_workers.emplace_back(&ServerThreadManager::TickWorkerThread, &server_thread_manager);
    }

    std::thread timer_thread(&ServerThreadManager::TimerThread, &server_thread_manager);
    std::thread metrics_thread(&ServerThreadManager::MetricsThread, &server_thread_manager);

    std::thread db_thread(&ServerThreadManager::DBThread, &server_thread_manager);
    if (!iocp_server->IsStressTestMode()) {
        iocp_server->RequestLoadRanking();
    }

    iocp_server->GetServerMetrics().created_thread_count.store(
        static_cast<std::uint64_t>(MAX_TICK_WORKERS + 3 + io_worker_count));

    for (int i = 0; i < io_worker_count; ++i)
        worker_threads.emplace_back(&ServerThreadManager::WorkerThread, &server_thread_manager);

    for (auto& th : worker_threads)
        th.join();

    for (auto& th : tick_workers)
        th.join();

    timer_thread.join();
    metrics_thread.join();
    db_thread.join();

    return 0;
}
