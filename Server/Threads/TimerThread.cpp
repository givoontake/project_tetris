#include <chrono>
#include "TimerThread.h"
#include "ServerThreadManager.h"
#include "../IOCPServer.h"

TimerThread::TimerThread(ServerThreadManager& manager) : manager(manager)
{
}

void TimerThread::Run()
{
    using clock = std::chrono::steady_clock;

    auto next_tick = clock::now();

    while (running.load() && manager.iocp_server.GetRunning())
    {
        // 다음 틱 시각 계산 후 그때까지 잠자기 cpu 내의 하드웨어 타이머를 통해 타이머 인터럽트가 발생하면 깨어나므로, 대기 중 CPU를 소모하지 않는다.
        next_tick += std::chrono::milliseconds(FRAME_TIME);
        {
            std::unique_lock<std::mutex> lock(wait_mutex);
            if (cv.wait_until(lock, next_tick, [this]() { return !running.load(); })) break;
        }
        if (!running.load() || !manager.iocp_server.GetRunning()) break;

        {
            std::lock_guard<std::mutex> lock(manager.g_tick_mutex);
            ++manager.g_tick_counter; // 새 틱 발생
        }

        // 새 틱이 생겼으니 TickWorker 들을 깨운다.
        manager.g_tick_cv.notify_all();
        manager.iocp_server.WakeDBWorkers();
    }

    if (running.load() && !manager.iocp_server.GetRunning()) manager.CloseThreads();
}

void TimerThread::Close()
{
    {
        std::lock_guard<std::mutex> lock(wait_mutex);
        running = false;
    }
    cv.notify_one();
}
