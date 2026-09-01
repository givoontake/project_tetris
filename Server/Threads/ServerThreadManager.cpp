#include <thread>
#include <utility>
#include "ServerThreadManager.h"
#include "IOCPServer.h"
#include "IOThread.h"
#include "TickThread.h"
#include "TickPhaseContext.h"
#include "TimerThread.h"

ServerThreadManager::ServerThreadManager(IOCPServer& iocp_server, TickWaitPolicy tick_policy)
    : iocp_server_(iocp_server), tick_wait_policy_(tick_policy), threads_(THREAD_GROUP_COUNT)
{
}

void ServerThreadManager::StartThreads()
{
    std::lock_guard<std::mutex> lock(thread_mutex_);
    int num_threads = std::thread::hardware_concurrency();
    // 혼합 대기를 사용하지 않으므로 매니저에서 첫 틱 시각을 만들어 공유하지 않는다.
    // const auto initial_next_tick = std::chrono::steady_clock::now() + std::chrono::milliseconds(FRAME_TIME);
    // next_tick_time_count.store(initial_next_tick.time_since_epoch().count());

    tick_threads_.reserve(MAX_TICK_WORKERS);
    for (int i = 0; i < MAX_TICK_WORKERS; ++i){
        auto tick_thread = std::make_unique<TickThread>(*this);
        tick_threads_.emplace_back(tick_thread.get());
        thread_objects_.emplace_back(std::move(tick_thread));
        thread_objects_.back()->Start();
        threads_[TICK_THREADS].emplace_back(&ServerThread::Run, thread_objects_.back().get());
        --num_threads;
    }

    thread_objects_.emplace_back(std::make_unique<TimerThread>(*this));
    thread_objects_.back()->Start();
    threads_[TIMER_THREADS].emplace_back(&ServerThread::Run, thread_objects_.back().get());
    --num_threads;

    iocp_server_.StartDBWorkers();
    for (int i = 0; i < GAME_DB_WORKER_COUNT; ++i) {
        threads_[DB_THREADS].emplace_back(&ServerThread::Run, &iocp_server_.game_db_workers_[i]);
    }
    for (int i = 0; i < LOGIN_DB_WORKER_COUNT; ++i) {
        threads_[DB_THREADS].emplace_back(&ServerThread::Run, &iocp_server_.login_db_worker_);
    }
    num_threads -= DB_WORKER_COUNT;
    iocp_server_.RequestLoadRanking();

    for (int i = 0; i < num_threads; ++i) {
        thread_objects_.emplace_back(std::make_unique<IOThread>(iocp_server_));
        thread_objects_.back()->Start();
        threads_[IO_THREADS].emplace_back(&ServerThread::Run, thread_objects_.back().get());
    }
}

int ServerThreadManager::GetCompleteTickWorkerCount() const
{
    int complete_worker_count = 0;
    for (const TickThread* tick_thread : tick_threads_) {
        if (tick_thread->state_.load() == ThreadState::COMPLETE) ++complete_worker_count;
    }
    return complete_worker_count;
}

std::vector<TickThread*> ServerThreadManager::SelectTickWorkers()
{
    std::vector<TickThread*> selected_tick_threads;
    selected_tick_threads.reserve(MAX_TICK_WORKERS);
    for (TickThread* tick_thread : tick_threads_) {
        ThreadState expected_state = ThreadState::COMPLETE;
        if (tick_thread->state_.compare_exchange_strong(expected_state, ThreadState::PROCESSING))
            selected_tick_threads.emplace_back(tick_thread);
    }
    return selected_tick_threads;
}

bool ServerThreadManager::StartTickPhase()
{
    if (!iocp_server_.GetRunning()) return false;
    const std::vector<TickThread*> selected_tick_threads = SelectTickWorkers();
    if (selected_tick_threads.size() < MIN_AVAILABLE_TICK_WORKERS) {
        for (TickThread* tick_thread : selected_tick_threads)
            tick_thread->state_.store(ThreadState::COMPLETE);
        return false;
    }

	// 일부 틱 스레드 작업이 완료되지 않아도 다음 틱으로 넘어갈 수 있으므로 틱 처리 시작에 항상 새 TickPhaseContext를 생성한다.
    const auto phase_context = std::make_shared<TickPhaseContext>(static_cast<int>(selected_tick_threads.size()));
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

void ServerThreadManager::CompleteTickPhase(TickThread& tick_thread, const std::shared_ptr<TickPhaseContext>& phase_context)
{
    tick_thread.phase_context_.reset();
    tick_thread.phase_.store(TickPhase::NONE);
    tick_thread.state_.store(ThreadState::COMPLETE);
    phase_context->completed_worker_count_.fetch_add(1);
}

void ServerThreadManager::CloseThreads()
{
    std::lock_guard<std::mutex> lock(thread_mutex_);
    if (is_closing_) return;
    is_closing_ = true;
    iocp_server_.is_running_ = false;

    for (auto& thread_object : thread_objects_)
        thread_object->Close();
}

void ServerThreadManager::JoinThreads()
{
    for (std::size_t group_index = 0; group_index < threads_.size(); ++group_index) {
        if (group_index == TICK_THREADS) CloseThreads();
        if (group_index == DB_THREADS) iocp_server_.StopDBWorkers();
        for (auto& th : threads_[group_index])
            th.join();
    }
}

