#pragma once
#include <memory>
#include <mutex>
#include <unordered_map>
#include <vector>
#include "Types.h"

class Session;
struct DBResultLogin;

class ActiveUserManager
{
	std::unordered_map<int, WP<Session>> active_users;
	mutable std::mutex active_users_mutex;

public:
	bool AddUser(const SP<Session>& session, DBResultLogin* login_result);
	void RemoveUser(int user_id, const SP<Session>& session);
	SP<Session> FindSessionById(int user_id) const;
	std::vector<SP<Session>> GetActiveSessions() const;
	void Clear();
};
