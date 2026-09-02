#pragma once
#include "DBThread.h"

class GameDBThread final : public DBThread
{
protected:
    void ProcessTask(DBTask& task) override;

private:
	void ExecuteLoadRankings();
    void ExecuteUpdateScore(SessionKey session_key, int new_score);
    void ExecuteUpdateMatchResult(const DBUpdateMatchResultTask& task);
    void ExecuteAddFriend(SessionKey session_key, FriendInfo acceptor_info, int requester_id);
	void ExecuteDeleteFriend(SessionKey session_key, int target_id);
    void ExecuteAddFriendRequest(SessionKey session_key, FriendInfo requester_info, int receiver_id);
	void ExecuteLoadFriendList(SessionKey session_key);
};
