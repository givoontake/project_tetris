#include <chrono>
#include <immintrin.h>
#include "TickThread.h"
#include "TickPhaseContext.h"
#include "ServerThreadManager.h"
#include "../IOCPServer.h"

TickThread::TickThread(ServerThreadManager& manager) : manager(manager)
{
}

void TickThread::Run()
{
    while (running.load() && manager.iocp_server.GetRunning())
    {
        WaitTickPhase();
        if (!running.load() || !manager.iocp_server.GetRunning()) break;

        auto current_phase_context = phase_context; // 틱이 밀려 있어도 새로운 처리를 전달받은 시점에 한 번에 처리

        while (true) {
            int i = current_phase_context->room_index_counter.fetch_add(1);
            if (i >= MAX_ROOM) break;

            auto room = manager.iocp_server.GetRoom(i);
            if (room) {
                ROOM_PROCESS_STATE expected_state = ROOM_PROCESS_STATE::COMPLETE;
                if (!room->processing_state.compare_exchange_strong(expected_state, ROOM_PROCESS_STATE::PROCESSING)) continue;
                room->ProcessPlayTasks();
                room->processing_state.store(ROOM_PROCESS_STATE::COMPLETE);
            }
        }

        manager.CompleteTickPhase(*this, current_phase_context);
    }
}

void TickThread::WaitTickPhase()
{
    // 혼합 대기는 공유된 다음 틱 시각을 기준으로 미리 깨어나지만, 현재는 사용하지 않는다.
    /*
    using clock = std::chrono::steady_clock;
    const auto next_tick_time = clock::time_point(clock::duration(manager.next_tick_time_count.load()));
    if (manager.tick_wait_policy == TICK_WAIT_POLICY::HYBRID_SPIN) {
        const auto spin_wait_start_time = next_tick_time - std::chrono::milliseconds(ServerThreadManager::TICK_SPIN_WAIT_MARGIN_MS);
        if (clock::now() < spin_wait_start_time) {
            std::unique_lock<std::mutex> lock(manager.g_tick_mutex);
            manager.g_tick_cv.wait_until(lock, spin_wait_start_time, [&] {
                return !running.load() || !manager.iocp_server.GetRunning() || phase.load() != TICK_PHASE::NONE;
            });
        }
    }
    */
    // 일반 수면 대기는 시각을 비교하지 않고, 작업 배정 또는 종료 알림을 기다리면 된다.
    // else if (manager.tick_wait_policy == TICK_WAIT_POLICY::FULL_SLEEP && clock::now() < next_tick_time) {
    if (manager.tick_wait_policy == TICK_WAIT_POLICY::FULL_SLEEP) {
        std::unique_lock<std::mutex> lock(manager.g_tick_mutex);
        // 람다 함수가 참을 반환해야 다음 코드가 진행된다. notify 관련 함수가 호출되면 조건 검사를 다시 수행한다.
        manager.g_tick_cv.wait(lock, [&] { // 전달한 lock으로 람다 내부 범위를 lock, 조건 검사하고 바로 unlock한다.
            if (!running.load() || !manager.iocp_server.GetRunning()) return true; // 종료 신호가 오면 바로 빠져나오기
            return phase.load() != TICK_PHASE::NONE;
            });
    }

    while (running.load() && manager.iocp_server.GetRunning() && phase.load() == TICK_PHASE::NONE)
        _mm_pause();
}

void TickThread::Close()
{
    {
        std::lock_guard<std::mutex> lock(manager.g_tick_mutex);
        running = false;
    }
    manager.g_tick_cv.notify_all();
}
