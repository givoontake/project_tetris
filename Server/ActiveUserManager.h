#pragma once
#include <memory>
#include <mutex>
#include <unordered_map>
#include <vector>

class Session;

class ActiveUserManager
{
	std::unordered_map<int, std::weak_ptr<Session>> active_users;
	mutable std::mutex active_users_mutex;

public:
	void AddUser(int user_id, const std::shared_ptr<Session>& session);
	void RemoveUser(int user_id, const std::shared_ptr<Session>& session);
	std::shared_ptr<Session> FindSessionById(int user_id) const;
	std::vector<std::shared_ptr<Session>> GetActiveSessions() const;
	bool IsActiveSession(int user_id) const;	
	void Clear();
};
