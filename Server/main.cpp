#include <iostream>
#include <vector>
#include <thread>
#include <chrono>
#include <mutex>
#include <condition_variable>
#include "IOCPServer.h"

IOCPServer iocp_server;

std::mutex              g_tick_mutex;
std::condition_variable g_tick_cv;

int  g_tick_counter = 0;
//bool tick_enable = false; 

void WorkerThread()
{
    iocp_server.ProcessGQCS();
}

void TickWorkerThread(int num)
{
    int thread_num = num;
    int last_tick = 0;

    while (iocp_server.GetRunning())
    {
        std::unique_lock<std::mutex> lock(g_tick_mutex);

        // 람다 함수가 참을 반환해야 다음 코드가 진행된다. notify 관련 함수가 호출되면 조건 검사를 다시 수행한다.
        g_tick_cv.wait(lock, [&] { // 전달한 lock으로 람다 내부 범위를 lock, 조건 검사하고 바로 unlock한다.
            if (!iocp_server.GetRunning()) return true; // 종료 신호가 오면 바로 빠져나오기
            return g_tick_counter > last_tick;
            });

        int current_tick = g_tick_counter; // 틱이 밀려 있어도 새로운 처리를 전달받은 시점에 한 번에 처리
        lock.unlock();

        for (int i = thread_num; i < MAX_ROOM; i += MAX_TICK_WORKERS) {
            
            if (iocp_server.GetRoom(i) && (iocp_server.GetRoom(i)->GetRoomState() == ROOM_STATE::PLAY)) { // 널이 아니고 플레이 중이면
                iocp_server.GetRoom(i)->ProcessPlayTasks();
                iocp_server.GetRoom(i)->UpdateTick();
            }
        }

        last_tick = current_tick;
    }
}

void TimerThread()
{
    using clock = std::chrono::steady_clock;

    auto next_tick = clock::now();

    while (iocp_server.GetRunning())
    {
        // 다음 틱 시각 계산 후 그때까지 잠자기 cpu 내의 하드웨어 타이머를 통해 타이머 인터럽트가 발생하면 깨어나므로, 대기 중 CPU를 소모하지 않는다.
        next_tick += std::chrono::milliseconds(FRAME_TIME);
        std::this_thread::sleep_until(next_tick);

        {
            std::lock_guard<std::mutex> lock(g_tick_mutex);
            ++g_tick_counter; // 새 틱 발생
        }

        // 새 틱이 생겼으니 TickWorker 들을 깨운다.
        g_tick_cv.notify_all();
    }
}

void DBThread()
{
    iocp_server.GetDB().Run();
}

int main()
{
    SetConsoleOutputCP(65001);

    iocp_server.StartServer();
    iocp_server.GetDB().init(iocp_server.GetHandle());

    std::vector<std::thread> worker_threads;
    int num_threads = std::thread::hardware_concurrency();

    std::vector<std::thread> tick_workers;
    for (int i = 0; i < MAX_TICK_WORKERS; ++i){
        tick_workers.emplace_back(TickWorkerThread, i);
        --num_threads;
    }

    std::thread timer_thread(TimerThread);
    --num_threads;

    std::thread db_thread(DBThread);
    --num_threads;

    for (int i = 0; i < num_threads; ++i)
        worker_threads.emplace_back(WorkerThread);

    for (auto& th : worker_threads)
        th.join();

    for (auto& th : tick_workers)
        th.join();

    timer_thread.join();
    db_thread.join();

    return 0;
}
