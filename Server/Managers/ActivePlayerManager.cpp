#include "ActivePlayerManager.h"
#include "Session.h"

bool ActivePlayerManager::AddPlayer(const SP<Session>& session, DBResultLogin* login_result)
{
	if (!session || !login_result) return false;
	std::lock_guard<std::mutex> lock(active_players_mutex_);
	auto it = active_players_.find(login_result->player_id);
	if (it != active_players_.end()) {
		if (!it->second.expired()) return false;
		active_players_.erase(it);
	}
	if (!session->ApplyLoginResult(login_result)) return false;
	active_players_[login_result->player_id] = session;
	return true;
}

void ActivePlayerManager::RemovePlayer(int player_id, const SP<Session>& session)
{
	std::lock_guard<std::mutex> lock(active_players_mutex_);
	auto it = active_players_.find(player_id);
	if (it == active_players_.end()) return;
	if (it->second.lock() != session) return;
	active_players_.erase(it);
}

SP<Session> ActivePlayerManager::FindSessionByID(int player_id) const
{
	std::lock_guard<std::mutex> lock(active_players_mutex_);
	auto it = active_players_.find(player_id);
	if (it == active_players_.end()) return nullptr;
	return it->second.lock();
}

std::vector<SP<Session>> ActivePlayerManager::GetActiveSessions() const
{
	std::vector<SP<Session>> sessions;
	std::lock_guard<std::mutex> lock(active_players_mutex_);
	sessions.reserve(active_players_.size());
	for (auto& active_player : active_players_) {
		if (auto session = active_player.second.lock()) sessions.emplace_back(session);
	}
	return sessions;
}

void ActivePlayerManager::Clear()
{
	std::lock_guard<std::mutex> lock(active_players_mutex_);
	active_players_.clear();
}
