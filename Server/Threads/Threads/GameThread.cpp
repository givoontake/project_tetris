#include <chrono>
#include <immintrin.h>
#include "GameThread.h"
#include "GamePhaseContext.h"
#include "GameThreadManager.h"
#include "IOCPServer.h"

GameThread::GameThread(GameThreadManager& manager) : manager_(manager)
{
}

void GameThread::Run()
{
    while (is_running_.load() && manager_.iocp_server_.IsRunning())
    {
        WaitGamePhase();
        if (!is_running_.load() || !manager_.iocp_server_.IsRunning()) break;

        auto current_phase_context = phase_context_; // 틱이 밀려 있어도 새로운 처리를 전달받은 시점에 한 번에 처리
		bool expected = false;
		if (current_phase_context->is_lifecycle_claimed.compare_exchange_strong(expected, true)) {
			bool is_lifecycle_acquired = false;
			while (is_running_.load() && manager_.iocp_server_.IsRunning()) {
				expected = false;
				if (manager_.is_lifecycle_processing_.compare_exchange_weak(expected, true)) {
					is_lifecycle_acquired = true;
					break;
				}
				_mm_pause();
			}
			if (is_lifecycle_acquired) {
				const std::size_t lifecycle_task_count = manager_.lifecycle_tasks_.ClaimTaskCount();
				for (std::size_t i = 0; i < lifecycle_task_count; ++i)
					manager_.ProcessTask(manager_.lifecycle_tasks_.Dequeue());
				manager_.is_lifecycle_processing_.store(false);
			}
			current_phase_context->is_lifecycle_complete.store(true);
		}
		while (is_running_.load() && manager_.iocp_server_.IsRunning() && !current_phase_context->is_lifecycle_complete.load())
			_mm_pause();

        while (true) {
            int i = current_phase_context->next_room_index.fetch_add(1);
            if (i >= MAX_ROOM_COUNT) break;

            auto room = manager_.iocp_server_.GetRoomByIndex(i);
            if (room) {
                RoomProcessState expected_state = RoomProcessState::COMPLETE;
                if (!room->processing_state_.compare_exchange_strong(expected_state, RoomProcessState::PROCESSING)) continue;
				room->ProcessRoomTick(current_phase_context->tick_time_ms);
				room->processing_state_.store(RoomProcessState::COMPLETE);
            }
        }

        phase_context_.reset();
        phase_.store(GamePhase::NONE);
        state_.store(GameThreadState::AVAILABLE);
    }
}

void GameThread::WaitGamePhase()
{
    // 혼합 대기는 공유된 다음 틱 시각을 기준으로 미리 깨어나지만, 현재는 사용하지 않는다.
    /*
    using Clock = std::chrono::steady_clock;
    const auto next_tick_time = Clock::time_point(Clock::duration(manager_.next_tick_time_count_.load()));
    if (manager_.tick_wait_policy_ == TickWaitPolicy::HYBRID_SPIN) {
        const auto spin_wait_start_time = next_tick_time - std::chrono::milliseconds(GameThreadManager::TICK_SPIN_WAIT_MARGIN_MS);
        if (Clock::now() < spin_wait_start_time) {
            std::unique_lock<std::mutex> lock(manager_.tick_mutex_);
            manager_.tick_cv_.wait_until(lock, spin_wait_start_time, [&] {
                return !is_running_.load() || !manager_.iocp_server_.IsRunning() || phase_.load() != GamePhase::NONE;
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
            return phase_.load() != GamePhase::NONE;
            });
    }

    while (is_running_.load() && manager_.iocp_server_.IsRunning() && phase_.load() == GamePhase::NONE)
        _mm_pause();
}

void GameThread::Close()
{
    {
        std::lock_guard<std::mutex> lock(manager_.tick_mutex_);
        is_running_ = false;
	}
	manager_.tick_cv_.notify_all();
}
