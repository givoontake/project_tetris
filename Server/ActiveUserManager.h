#pragma once
#include <mutex>
#include <unordered_map>

class ActiveUserManager
{
	std::unordered_map<int, int> active_users;
	mutable std::mutex active_users_mutex;

public:
	void AddUser(int user_id, int session_index);
	void RemoveUser(int user_id, int session_index);
	int FindSessionIndexById(int user_id) const;
	void Clear();
};
