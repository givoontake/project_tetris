#include <thread>
#include <utility>
#include "ServerThreadManager.h"
#include "IOCPServer.h"
#include "IOThread.h"
#include "TickThread.h"
#include "TickPhaseContext.h"
#include "TimerThread.h"

ServerThreadManager::ServerThreadManager(IOCPServer& iocp_server, TICK_WAIT_POLICY tick_policy)
    : iocp_server(iocp_server), tick_wait_policy(tick_policy), threads(THREAD_GROUP_COUNT)
{
}

void ServerThreadManager::StartThreads()
{
    std::lock_guard<std::mutex> lock(thread_mutex);
    int num_threads = std::thread::hardware_concurrency();
    // 혼합 대기를 사용하지 않으므로 매니저에서 첫 틱 시각을 만들어 공유하지 않는다.
    // const auto initial_next_tick = std::chrono::steady_clock::now() + std::chrono::milliseconds(FRAME_TIME);
    // next_tick_time_count.store(initial_next_tick.time_since_epoch().count());

    tick_threads.reserve(MAX_TICK_WORKERS);
    for (int i = 0; i < MAX_TICK_WORKERS; ++i){
        auto tick_thread = std::make_unique<TickThread>(*this);
        tick_threads.emplace_back(tick_thread.get());
        thread_objects.emplace_back(std::move(tick_thread));
        thread_objects.back()->Start();
        threads[TICK_THREADS].emplace_back(&ServerThread::Run, thread_objects.back().get());
        --num_threads;
    }

    thread_objects.emplace_back(std::make_unique<TimerThread>(*this));
    thread_objects.back()->Start();
    threads[TIMER_THREADS].emplace_back(&ServerThread::Run, thread_objects.back().get());
    --num_threads;

    iocp_server.StartDBWorkers();
    for (int i = 0; i < GAME_DB_WORKER_COUNT; ++i) {
        threads[DB_THREADS].emplace_back(&ServerThread::Run, &iocp_server.game_db_workers[i]);
    }
    for (int i = 0; i < LOGIN_DB_WORKER_COUNT; ++i) {
        threads[DB_THREADS].emplace_back(&ServerThread::Run, &iocp_server.login_db_worker);
    }
    num_threads -= DB_WORKER_COUNT;
    iocp_server.RequestLoadRanking();

    for (int i = 0; i < num_threads; ++i) {
        thread_objects.emplace_back(std::make_unique<IOThread>(iocp_server));
        thread_objects.back()->Start();
        threads[IO_THREADS].emplace_back(&ServerThread::Run, thread_objects.back().get());
    }
}

int ServerThreadManager::GetCompleteTickWorkerCount() const
{
    int complete_worker_count = 0;
    for (const TickThread* tick_thread : tick_threads) {
        if (tick_thread->state.load() == THREAD_STATE::COMPLETE) ++complete_worker_count;
    }
    return complete_worker_count;
}

std::vector<TickThread*> ServerThreadManager::SelectTickWorkers()
{
    std::vector<TickThread*> selected_tick_threads;
    selected_tick_threads.reserve(MAX_TICK_WORKERS);
    for (TickThread* tick_thread : tick_threads) {
        THREAD_STATE expected_state = THREAD_STATE::COMPLETE;
        if (tick_thread->state.compare_exchange_strong(expected_state, THREAD_STATE::PROCESSING))
            selected_tick_threads.emplace_back(tick_thread);
    }
    return selected_tick_threads;
}

bool ServerThreadManager::StartTickPhase()
{
    if (!iocp_server.GetRunning()) return false;
    const std::vector<TickThread*> selected_tick_threads = SelectTickWorkers();
    if (selected_tick_threads.size() < MIN_AVAILABLE_TICK_WORKERS) {
        for (TickThread* tick_thread : selected_tick_threads)
            tick_thread->state.store(THREAD_STATE::COMPLETE);
        return false;
    }

	// 일부 틱 스레드 작업이 완료되지 않아도 다음 틱으로 넘어갈 수 있으므로 틱 처리 시작에 항상 새 TickPhaseContext를 생성한다.
    const auto phase_context = std::make_shared<TickPhaseContext>(static_cast<int>(selected_tick_threads.size()));
    {
        std::lock_guard<std::mutex> lock(g_tick_mutex);
        for (TickThread* tick_thread : selected_tick_threads) {
            tick_thread->phase_context = phase_context;
            tick_thread->phase.store(TICK_PHASE::ROOM_PROCESS);
        }
    }
    g_tick_cv.notify_all();
    return true;
}

void ServerThreadManager::CompleteTickPhase(TickThread& tick_thread, const std::shared_ptr<TickPhaseContext>& phase_context)
{
    tick_thread.phase_context.reset();
    tick_thread.phase.store(TICK_PHASE::NONE);
    tick_thread.state.store(THREAD_STATE::COMPLETE);
    phase_context->completed_worker_count.fetch_add(1);
}

void ServerThreadManager::CloseThreads()
{
    std::lock_guard<std::mutex> lock(thread_mutex);
    if (closing) return;
    closing = true;
    iocp_server.is_running = false;

    for (auto& thread_object : thread_objects)
        thread_object->Close();
}

void ServerThreadManager::JoinThreads()
{
    for (std::size_t group_index = 0; group_index < threads.size(); ++group_index) {
        if (group_index == TICK_THREADS) CloseThreads();
        if (group_index == DB_THREADS) iocp_server.StopDBWorkers();
        for (auto& th : threads[group_index])
            th.join();
    }
}

