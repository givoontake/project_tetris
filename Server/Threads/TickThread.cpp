#include "TickThread.h"
#include "ServerThreadManager.h"
#include "../IOCPServer.h"

TickThread::TickThread(ServerThreadManager& manager, int num)
    : manager(manager), thread_num(num)
{
}

void TickThread::Run()
{
    int last_tick = 0;

    while (running.load() && manager.iocp_server.GetRunning())
    {
        std::unique_lock<std::mutex> lock(manager.g_tick_mutex);

        // 람다 함수가 참을 반환해야 다음 코드가 진행된다. notify 관련 함수가 호출되면 조건 검사를 다시 수행한다.
        manager.g_tick_cv.wait(lock, [&] { // 전달한 lock으로 람다 내부 범위를 lock, 조건 검사하고 바로 unlock한다.
            if (!running.load() || !manager.iocp_server.GetRunning()) return true; // 종료 신호가 오면 바로 빠져나오기
            return manager.g_tick_counter > last_tick;
            });

        if (!running.load() || !manager.iocp_server.GetRunning()) break;

        int current_tick = manager.g_tick_counter; // 틱이 밀려 있어도 새로운 처리를 전달받은 시점에 한 번에 처리
        lock.unlock();

        for (int i = thread_num; i < MAX_ROOM; i += ServerThreadManager::MAX_TICK_WORKERS) {
            auto room = manager.iocp_server.GetRoom(i);
            if (room) {
                room->ProcessPlayTasks();
            }
        }

        last_tick = current_tick;
    }
}

void TickThread::Close()
{
    {
        std::lock_guard<std::mutex> lock(manager.g_tick_mutex);
        running = false;
    }
    manager.g_tick_cv.notify_all();
}
