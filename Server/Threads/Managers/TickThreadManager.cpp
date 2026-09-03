#include "TickThreadManager.h"
#include "IOCPServer.h"
#include "TickPhaseContext.h"

TickThreadManager::TickThreadManager(IOCPServer& iocp_server, TickWaitPolicy tick_policy)
    : iocp_server_(iocp_server), tick_wait_policy_(tick_policy)
{
}

void TickThreadManager::Start()
{
    // 혼합 대기를 사용하지 않으므로 매니저에서 첫 틱 시각을 만들어 공유하지 않는다.
    // const auto initial_next_tick = std::chrono::steady_clock::now() + std::chrono::milliseconds(TICK_INTERVAL_MS);
    // next_tick_time_count.store(initial_next_tick.time_since_epoch().count());
    thread_objects_.reserve(THREAD_COUNT);
    threads_.reserve(THREAD_COUNT);
    for (int i = 0; i < THREAD_COUNT; ++i) {
        thread_objects_.emplace_back(std::make_unique<TickThread>(*this));
        thread_objects_.back()->Start();
        threads_.emplace_back(&ServerThread::Run, thread_objects_.back().get());
    }
}

int TickThreadManager::GetAvailableThreadCount() const
{
    int available_thread_count = 0;
    for (const auto& tick_thread : thread_objects_) {
        if (tick_thread->state_.load() == TickThreadState::AVAILABLE) ++available_thread_count;
    }
    return available_thread_count;
}

std::vector<TickThread*> TickThreadManager::SelectThreads()
{
    std::vector<TickThread*> selected_tick_threads;
    selected_tick_threads.reserve(THREAD_COUNT);
    for (const auto& tick_thread : thread_objects_) {
        TickThreadState expected_state = TickThreadState::AVAILABLE;
        if (tick_thread->state_.compare_exchange_strong(expected_state, TickThreadState::PROCESSING))
            selected_tick_threads.emplace_back(tick_thread.get());
    }
    return selected_tick_threads;
}

bool TickThreadManager::StartTickPhase(long long tick_time_ms)
{
    if (!iocp_server_.IsRunning()) return false;
    const std::vector<TickThread*> selected_tick_threads = SelectThreads();
    if (selected_tick_threads.size() < MIN_AVAILABLE_THREAD_COUNT) {
        for (TickThread* tick_thread : selected_tick_threads)
            tick_thread->state_.store(TickThreadState::AVAILABLE);
        return false;
    }

    // 일부 틱 스레드 작업이 완료되지 않아도 다음 틱으로 넘어갈 수 있으므로 틱 처리 시작에 항상 새 TickPhaseContext를 생성한다.
    const auto phase_context = std::make_shared<TickPhaseContext>(tick_time_ms);
    {
        std::lock_guard<std::mutex> lock(tick_mutex_);
        for (TickThread* tick_thread : selected_tick_threads) {
            tick_thread->phase_context_ = phase_context;
            tick_thread->phase_.store(TickPhase::ROOM_PROCESS);
        }
    }
    tick_cv_.notify_all();
    return true;
}

void TickThreadManager::Close()
{
    for (auto& thread_object : thread_objects_)
        thread_object->Close();
}

void TickThreadManager::Join()
{
    for (auto& thread : threads_)
        thread.join();
}
