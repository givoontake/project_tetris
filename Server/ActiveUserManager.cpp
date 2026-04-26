#include "ActiveUserManager.h"
#include "Session.h"

void ActiveUserManager::AddUser(int user_id, const std::shared_ptr<Session>& session)
{
	std::lock_guard<std::mutex> lock(active_users_mutex);
	active_users[user_id] = session;
}

void ActiveUserManager::RemoveUser(int user_id, const std::shared_ptr<Session>& session)
{
	std::lock_guard<std::mutex> lock(active_users_mutex);
	auto it = active_users.find(user_id);
	if (it == active_users.end()) return;
	if (it->second.lock() != session) return;
	active_users.erase(it);
}

std::shared_ptr<Session> ActiveUserManager::FindSessionById(int user_id) const
{
	std::lock_guard<std::mutex> lock(active_users_mutex);
	auto it = active_users.find(user_id);
	if (it == active_users.end()) return nullptr;
	return it->second.lock();
}

std::vector<std::shared_ptr<Session>> ActiveUserManager::GetActiveSessions() const
{
	std::vector<std::shared_ptr<Session>> sessions;
	std::lock_guard<std::mutex> lock(active_users_mutex);
	sessions.reserve(active_users.size());
	for (auto& active_user : active_users) {
		if (auto session = active_user.second.lock()) sessions.emplace_back(session);
	}
	return sessions;
}

bool ActiveUserManager::IsActiveSession(int user_id) const
{
	std::lock_guard<std::mutex> lock(active_users_mutex);
	auto it = active_users.find(user_id);
	if (it != active_users.end() && !it->second.expired()) return true;
	return false;
}

void ActiveUserManager::Clear()
{
	std::lock_guard<std::mutex> lock(active_users_mutex);
	active_users.clear();
}
