#pragma once
#include <memory>
#include <mutex>
#include <unordered_map>
#include <vector>
#include "types.h"

class Session;
struct DBResultLogin;

class ActivePlayerManager
{
	std::unordered_map<int, WP<Session>> active_players_;
	mutable std::mutex active_players_mutex_;

public:
	bool AddPlayer(const SP<Session>& session, DBResultLogin* login_result);
	void RemovePlayer(int player_id, const SP<Session>& session);
	SP<Session> FindSessionByID(int player_id) const;
	std::vector<SP<Session>> GetActiveSessions() const;
	void Clear();
};
