#include <chrono>
#include <immintrin.h>
#include "TickThread.h"
#include "TickPhaseContext.h"
#include "ServerThreadManager.h"
#include "IOCPServer.h"

TickThread::TickThread(ServerThreadManager& manager) : manager_(manager)
{
}

void TickThread::Run()
{
    while (is_running_.load() && manager_.iocp_server_.IsRunning())
    {
        WaitTickPhase();
        if (!is_running_.load() || !manager_.iocp_server_.IsRunning()) break;

        auto current_phase_context = phase_context_; // 틱이 밀려 있어도 새로운 처리를 전달받은 시점에 한 번에 처리

        while (true) {
            int i = current_phase_context->next_room_index_.fetch_add(1);
            if (i >= MAX_ROOM_COUNT) break;

            auto room = manager_.iocp_server_.GetRoomByIndex(i);
            if (room) {
                RoomProcessState expected_state = RoomProcessState::COMPLETE;
                if (!room->processing_state_.compare_exchange_strong(expected_state, RoomProcessState::PROCESSING)) continue;
				room->ProcessRoomTick();
				room->processing_state_.store(RoomProcessState::COMPLETE);
            }
        }

        manager_.CompleteTickThreadPhase(*this, current_phase_context);
    }
}

void TickThread::WaitTickPhase()
{
    // 혼합 대기는 공유된 다음 틱 시각을 기준으로 미리 깨어나지만, 현재는 사용하지 않는다.
    /*
    using Clock = std::chrono::steady_clock;
    const auto next_tick_time = Clock::time_point(Clock::duration(manager_.next_tick_time_count_.load()));
    if (manager_.tick_wait_policy_ == TickWaitPolicy::HYBRID_SPIN) {
        const auto spin_wait_start_time = next_tick_time - std::chrono::milliseconds(ServerThreadManager::TICK_SPIN_WAIT_MARGIN_MS);
        if (Clock::now() < spin_wait_start_time) {
            std::unique_lock<std::mutex> lock(manager_.tick_mutex_);
            manager_.tick_cv_.wait_until(lock, spin_wait_start_time, [&] {
                return !is_running_.load() || !manager_.iocp_server_.IsRunning() || phase_.load() != TickPhase::NONE;
            });
        }
    }
    */
    // 일반 수면 대기는 시각을 비교하지 않고, 작업 배정 또는 종료 알림을 기다리면 된다.
    // else if (manager_.tick_wait_policy_ == TickWaitPolicy::FULL_SLEEP && Clock::now() < next_tick_time) {
    if (manager_.tick_wait_policy_ == TickWaitPolicy::FULL_SLEEP) {
        std::unique_lock<std::mutex> lock(manager_.tick_mutex_);
        // 람다 함수가 참을 반환해야 다음 코드가 진행된다. notify 관련 함수가 호출되면 조건 검사를 다시 수행한다.
        manager_.tick_cv_.wait(lock, [&] { // 전달한 lock으로 람다 내부 범위를 lock, 조건 검사하고 바로 unlock한다.
            if (!is_running_.load() || !manager_.iocp_server_.IsRunning()) return true; // 종료 신호가 오면 바로 빠져나오기
            return phase_.load() != TickPhase::NONE;
            });
    }

    while (is_running_.load() && manager_.iocp_server_.IsRunning() && phase_.load() == TickPhase::NONE)
        _mm_pause();
}

void TickThread::Close()
{
    {
        std::lock_guard<std::mutex> lock(manager_.tick_mutex_);
        is_running_ = false;
	}
	manager_.tick_cv_.notify_all();
}
