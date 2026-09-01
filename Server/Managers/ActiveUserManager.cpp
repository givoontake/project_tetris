#include "ActiveUserManager.h"
#include "Session.h"

bool ActiveUserManager::AddUser(const SP<Session>& session, DBResultLogin* login_result)
{
	if (!session || !login_result) return false;
	std::lock_guard<std::mutex> lock(active_users_mutex_);
	auto it = active_users_.find(login_result->id);
	if (it != active_users_.end()) {
		if (!it->second.expired()) return false;
		active_users_.erase(it);
	}
	if (!session->InitDBInfo(login_result)) return false;
	active_users_[login_result->id] = session;
	return true;
}

void ActiveUserManager::RemoveUser(int user_id, const SP<Session>& session)
{
	std::lock_guard<std::mutex> lock(active_users_mutex_);
	auto it = active_users_.find(user_id);
	if (it == active_users_.end()) return;
	if (it->second.lock() != session) return;
	active_users_.erase(it);
}

SP<Session> ActiveUserManager::FindSessionById(int user_id) const
{
	std::lock_guard<std::mutex> lock(active_users_mutex_);
	auto it = active_users_.find(user_id);
	if (it == active_users_.end()) return nullptr;
	return it->second.lock();
}

std::vector<SP<Session>> ActiveUserManager::GetActiveSessions() const
{
	std::vector<SP<Session>> sessions;
	std::lock_guard<std::mutex> lock(active_users_mutex_);
	sessions.reserve(active_users_.size());
	for (auto& active_user : active_users_) {
		if (auto session = active_user.second.lock()) sessions.emplace_back(session);
	}
	return sessions;
}

void ActiveUserManager::Clear()
{
	std::lock_guard<std::mutex> lock(active_users_mutex_);
	active_users_.clear();
}
