#include "ActiveUserManager.h"

void ActiveUserManager::AddUser(int user_id, int session_index)
{
	std::lock_guard<std::mutex> lock(active_users_mutex);
	active_users[user_id] = session_index;
}

void ActiveUserManager::RemoveUser(int user_id, int session_index)
{
	std::lock_guard<std::mutex> lock(active_users_mutex);
	auto it = active_users.find(user_id);
	if (it == active_users.end()) return;
	if (it->second != session_index) return;
	active_users.erase(it);
}

int ActiveUserManager::FindSessionIndexById(int user_id) const
{
	std::lock_guard<std::mutex> lock(active_users_mutex);
	auto it = active_users.find(user_id);
	if (it == active_users.end()) return -1;
	return it->second;
}

bool ActiveUserManager::IsActiveSession(int user_id) const
{
	std::lock_guard<std::mutex> lock(active_users_mutex);
	auto it = active_users.find(user_id);
	if (it != active_users.end()) return true;
	return false;
}

void ActiveUserManager::Clear()
{
	std::lock_guard<std::mutex> lock(active_users_mutex);
	active_users.clear();
}
