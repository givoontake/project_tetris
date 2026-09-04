#include "GameThreadManager.h"
#include "IOCPServer.h"
#include "GamePhaseContext.h"

GameThreadManager::GameThreadManager(IOCPServer& iocp_server, TickWaitPolicy tick_policy)
    : iocp_server_(iocp_server), tick_wait_policy_(tick_policy)
{
}

void GameThreadManager::Start()
{
    // 혼합 대기를 사용하지 않으므로 매니저에서 첫 틱 시각을 만들어 공유하지 않는다.
    // const auto initial_next_tick = std::chrono::steady_clock::now() + std::chrono::milliseconds(TICK_INTERVAL_MS);
    // next_tick_time_count.store(initial_next_tick.time_since_epoch().count());
    thread_objects_.reserve(THREAD_COUNT);
    threads_.reserve(THREAD_COUNT);
    for (int i = 0; i < THREAD_COUNT; ++i) {
        thread_objects_.emplace_back(std::make_unique<GameThread>(*this));
        thread_objects_.back()->Start();
        threads_.emplace_back(&ServerThread::Run, thread_objects_.back().get());
    }
}

int GameThreadManager::GetAvailableThreadCount() const
{
    int available_thread_count = 0;
    for (const auto& game_thread : thread_objects_) {
        if (game_thread->state_.load() == GameThreadState::AVAILABLE) ++available_thread_count;
    }
    return available_thread_count;
}

std::vector<GameThread*> GameThreadManager::SelectThreads()
{
    std::vector<GameThread*> selected_game_threads;
    selected_game_threads.reserve(THREAD_COUNT);
    for (const auto& game_thread : thread_objects_) {
        GameThreadState expected_state = GameThreadState::AVAILABLE;
        if (game_thread->state_.compare_exchange_strong(expected_state, GameThreadState::PROCESSING))
            selected_game_threads.emplace_back(game_thread.get());
    }
    return selected_game_threads;
}

bool GameThreadManager::StartGamePhase(long long tick_time_ms)
{
    if (!iocp_server_.IsRunning()) return false;
    const std::vector<GameThread*> selected_game_threads = SelectThreads();
    if (selected_game_threads.size() < MIN_AVAILABLE_THREAD_COUNT) {
        for (GameThread* game_thread : selected_game_threads)
            game_thread->state_.store(GameThreadState::AVAILABLE);
        return false;
    }

    // 일부 게임 스레드 작업이 완료되지 않아도 다음 틱으로 넘어갈 수 있으므로 게임 처리 시작에 항상 새 GamePhaseContext를 생성한다.
	const auto phase_context = std::make_shared<GamePhaseContext>(tick_time_ms);
    {
        std::lock_guard<std::mutex> lock(tick_mutex_);
        for (GameThread* game_thread : selected_game_threads) {
            game_thread->phase_context_ = phase_context;
            game_thread->phase_.store(GamePhase::ROOM_PROCESS);
        }
    }
    tick_cv_.notify_all();
    return true;
}

void GameThreadManager::Enqueue(std::unique_ptr<RoomLifecycleTask> task)
{
	lifecycle_tasks_.Enqueue(std::move(task));
}

void GameThreadManager::Close()
{
    for (auto& thread_object : thread_objects_)
        thread_object->Close();
}

void GameThreadManager::Join()
{
    for (auto& thread : threads_)
        thread.join();
}
