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

    std::vector<std::thread> tick_workers;
    for (int i = 0; i < MAX_TICK_WORKERS; ++i){
        tick_workers.emplace_back(&ServerThreadManager::TickWorkerThread, &server_thread_manager, i);
        --num_threads;
    }

    std::thread timer_thread(&ServerThreadManager::TimerThread, &server_thread_manager);
    --num_threads;

    std::thread db_thread(&ServerThreadManager::DBThread, &server_thread_manager);
    --num_threads;
    if (!iocp_server->IsStressTestMode()) {
        iocp_server->RequestLoadRanking();
    }

    for (int i = 0; i < num_threads; ++i)
        worker_threads.emplace_back(&ServerThreadManager::WorkerThread, &server_thread_manager);

    for (auto& th : worker_threads)
        th.join();

    for (auto& th : tick_workers)
        th.join();

    timer_thread.join();
    db_thread.join();

    return 0;
}
