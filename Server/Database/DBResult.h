#pragma once
#include <string>
#include <vector>
#include "enum_class.h"

struct DBResultBase {
	// 유니크 포인터용 껍데기 구조체
	// void*는 삭제가 불가능하므로, 삭제가 가능한 껍데기 구조체로 관리
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
	std::string login_id; // 데이터베이스 접속 시 우선 키로 활용

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

struct DBResultAddFriend : public DBResultBase { // 재조회하기는 싫으니까 그냥 닉네임을 받는걸로 하자
	// DB 요청 이후 두 세션은 존재하는지, 아닌지, 재로그인 했는지 알 수 없으므로 어차피 탐색해서 찾아야 함
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
