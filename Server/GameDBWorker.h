#pragma once
#include "Database.h"

class GameDBWorker final : public Database
{
protected:
    void ProcessTask(DBTask& task) override;

private:
    void ExecuteLoadRanking();
    void ExecuteUpdateScore(SessionKey key, int new_score);
    void ExecuteUpdateMatchResult(const DBUpdateMatchResultTask& task);
    void ExecuteAddFriend(SessionKey key, FriendInfo accepter_info, int requester_id);
	void ExecuteDeleteFriend(SessionKey key, int target_id);
    void ExecuteAddFriendRequest(SessionKey key, FriendInfo requester_info, int recver_id);
	void ExecuteLoadFriendList(SessionKey key);
};
