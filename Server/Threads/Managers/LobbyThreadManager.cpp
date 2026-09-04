#include "LobbyThreadManager.h"
#include "IOCPServer.h"
#include "LobbyPhaseContext.h"

LobbyThreadManager::LobbyThreadManager(IOCPServer& iocp_server) : iocp_server_(iocp_server)
{
}

void LobbyThreadManager::Start()
{
	thread_objects_.reserve(THREAD_COUNT);
	threads_.reserve(THREAD_COUNT);
	for (int i = 0; i < THREAD_COUNT; ++i) {
		thread_objects_.emplace_back(std::make_unique<LobbyThread>(*this));
		thread_objects_.back()->Start();
		threads_.emplace_back(&ServerThread::Run, thread_objects_.back().get());
	}
}

void LobbyThreadManager::StartLobbyPhase()
{
	if (!iocp_server_.IsRunning()) return;
	for (const auto& lobby_thread : thread_objects_) {
		if (lobby_thread->state_.load() != LobbyThreadState::AVAILABLE) return;
	}

	const auto phase_context = std::make_shared<LobbyPhaseContext>(lifecycle_tasks_.ClaimTaskCount());
	{
		std::lock_guard<std::mutex> lock(lobby_mutex_);
		for (const auto& lobby_thread : thread_objects_) {
			lobby_thread->state_.store(LobbyThreadState::PROCESSING);
			lobby_thread->phase_context_ = phase_context;
			lobby_thread->phase_.store(LobbyPhase::SESSION_PROCESS);
		}
	}
	lobby_cv_.notify_all();
}

void LobbyThreadManager::Enqueue(std::unique_ptr<LobbyTask> task)
{
	lifecycle_tasks_.Enqueue(std::move(task));
}

LobbySession* LobbyThreadManager::GetLobbySession(int session_index)
{
	if (session_index < 0 || session_index >= MAX_PLAYER_COUNT) return nullptr;
	return &lobby_sessions_[session_index];
}

void LobbyThreadManager::Close()
{
	for (auto& thread_object : thread_objects_)
		thread_object->Close();
}

void LobbyThreadManager::Join()
{
	for (auto& thread : threads_)
		thread.join();
}
