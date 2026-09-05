#include <immintrin.h>
#include "LobbyThread.h"
#include "LobbyPhaseContext.h"
#include "LobbyThreadManager.h"
#include "TetrisServer.h"
#include "Session.h"

LobbyThread::LobbyThread(LobbyThreadManager& manager) : manager_(manager)
{
}

void LobbyThread::Run()
{
	while (is_running_.load() && manager_.tetris_server_.IsRunning()) {
		WaitLobbyPhase();
		if (!is_running_.load() || !manager_.tetris_server_.IsRunning()) break;

		auto current_phase_context = phase_context_;
		bool expected = false;
		if (current_phase_context->is_lifecycle_claimed.compare_exchange_strong(expected, true)) {
			for (std::size_t i = 0; i < current_phase_context->lifecycle_task_count; ++i)
				manager_.ProcessTask(manager_.lifecycle_tasks_.Dequeue());
			current_phase_context->is_lifecycle_complete.store(true);
		}
		while (is_running_.load() && manager_.tetris_server_.IsRunning() && !current_phase_context->is_lifecycle_complete.load())
			_mm_pause();

		while (is_running_.load() && manager_.tetris_server_.IsRunning()) {
			const int session_index = current_phase_context->next_session_index.fetch_add(1);
			if (session_index >= MAX_PLAYER_COUNT) break;

			auto& lobby_session = manager_.lobby_sessions_[session_index];
			if (lobby_session.GetState() != ActiveEntryState::ACTIVE) continue;
			auto* session = manager_.tetris_server_.FindSessionByIndex(session_index);
			if (!session || session->GetLifeState() != LifeState::ACTIVE) continue;
			const SessionKey session_key = session->GetSessionKey();
			if (!lobby_session.MatchesSessionKey(session_key)) continue;
			ModeState mode_state = session->GetModeState();
			if (mode_state != ModeState::LOGIN && mode_state != ModeState::LOBBY) continue;
			if (!session->TryStartTaskProcessing()) continue;

			mode_state = session->GetModeState();
			if (lobby_session.GetState() != ActiveEntryState::ACTIVE || !lobby_session.MatchesSessionKey(session_key) || session->GetLifeState() != LifeState::ACTIVE || (mode_state != ModeState::LOGIN && mode_state != ModeState::LOBBY)) {
				session->CompleteTaskProcessing();
				continue;
			}

			const std::size_t task_count = session->ClaimTaskCount();
			for (std::size_t i = 0; i < task_count; ++i) {
				const SessionTaskProcessResult result = manager_.tetris_server_.ProcessSessionTask(*session, session->DequeueTask());
				if (result == SessionTaskProcessResult::DISCARD_REMAINING) {
					for (++i; i < task_count; ++i) session->DequeueTask();
					session->DiscardTasks();
					break;
				}

				mode_state = session->GetModeState();
				if (result == SessionTaskProcessResult::RESTORE_REMAINING || lobby_session.GetState() != ActiveEntryState::ACTIVE || !lobby_session.MatchesSessionKey(session_key) || session->GetLifeState() != LifeState::ACTIVE || (mode_state != ModeState::LOGIN && mode_state != ModeState::LOBBY)) {
					session->RestoreClaimedTaskCount(task_count - i - 1);
					break;
				}
			}
			session->CompleteTaskProcessing();
		}

		phase_context_.reset();
		phase_.store(LobbyPhase::NONE);
		state_.store(LobbyThreadState::AVAILABLE);
	}
}

void LobbyThread::WaitLobbyPhase()
{
	std::unique_lock<std::mutex> lock(manager_.lobby_mutex_);
	manager_.lobby_cv_.wait(lock, [&] {
		return !is_running_.load() || !manager_.tetris_server_.IsRunning() || phase_.load() != LobbyPhase::NONE;
	});
}

void LobbyThread::Close()
{
	{
		std::lock_guard<std::mutex> lock(manager_.lobby_mutex_);
		is_running_ = false;
	}
	manager_.lobby_cv_.notify_all();
}
