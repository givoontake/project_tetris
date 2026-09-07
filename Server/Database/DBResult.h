#pragma once
#include <string>
#include <vector>
#include "enum_class.h"

struct DBResultBase {
	DBOperationType operation_type;
	bool is_success;

	DBResultBase(DBOperationType result_type, bool is_success)
		: operation_type(result_type), is_success(is_success) {}
	virtual ~DBResultBase() = default;
};

struct DBResultFailure final : public DBResultBase {
	DBResultFailure(DBOperationType result_type) : DBResultBase(result_type, false) {}
};

struct DBResultLogin : public DBResultBase {
	DBResultLogin() : DBResultBase(DBOperationType::LOGIN, true) {}

	int player_id;
	int max_score;
	int win_count;
	int lose_count;
	std::string nickname;
	std::string login_id; // 로그인 결과를 식별하는 키로 사용한다.

	void Clear()
	{
		player_id = -1;
		max_score = 0;
		win_count = 0;
		lose_count = 0;
		nickname.clear();
		login_id.clear();
	}
};

struct DBResultUpdateScore : public DBResultBase {
	DBResultUpdateScore() : DBResultBase(DBOperationType::UPDATE_SCORE, true) {}

	int max_score;
};

struct DBResultUpdateMatchResult : public DBResultBase {
	DBResultUpdateMatchResult() : DBResultBase(DBOperationType::UPDATE_MATCH_RESULT, true) {}

	bool is_winner;
};

struct FriendInfo {
	int player_id;
	std::string nickname;
};

struct RankingInfo {
	int player_id;
	std::string nickname;
	int score;
};

struct DBResultAddFriend : public DBResultBase {
	// DB 응답 시점의 세션 상태를 보장할 수 없으므로 식별자로 다시 조회한다.
	DBResultAddFriend() : DBResultBase(DBOperationType::ADD_FRIEND, true) {}

	FriendInfo requester_info;
	FriendInfo acceptor_info;
};

struct DBResultDeleteFriend : public DBResultBase {
	DBResultDeleteFriend() : DBResultBase(DBOperationType::DELETE_FRIEND, true) {}

	int requester_id;
	int target_id;
};

struct DBResultAddFriendRequest : public DBResultBase {
	DBResultAddFriendRequest() : DBResultBase(DBOperationType::ADD_FRIEND_REQUEST, true) {}

	FriendInfo requester_info;
	FriendInfo receiver_info;
};

struct DBResultLoadFriendList : public DBResultBase {
	DBResultLoadFriendList() : DBResultBase(DBOperationType::LOAD_FRIEND_LIST, true) {}

	std::vector<FriendInfo> friend_list;
};

struct DBResultLoadRankings : public DBResultBase {
	DBResultLoadRankings() : DBResultBase(DBOperationType::LOAD_RANKINGS, true) {}

	std::vector<RankingInfo> rankings;
};
