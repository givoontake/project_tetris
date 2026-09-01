#pragma once
#include <string>
#include <vector>
#include "define_packets.h"
#include "enum_class.h"

struct DBResultDefault {
	// 유니크 포인터용 껍데기 구조체
	// void*는 삭제가 불가능하므로, 삭제가 가능한 껍데기 구조체로 관리
	DBOperationType type;
	bool is_success;

	DBResultDefault(DBOperationType result_type, bool is_success)
		: type(result_type), is_success(is_success) {}
	virtual ~DBResultDefault() = default;
};

struct DBResultFailure final : public DBResultDefault {
	DBResultFailure(DBOperationType result_type) : DBResultDefault(result_type, false) {}
};

struct DBResultLogin : public DBResultDefault {
	DBResultLogin() : DBResultDefault(DBOperationType::LOGIN, true) {}

	int id;
	int max_score;
	int win_count;
	int lose_count;
	std::string nickname;
	std::string login_id; // 데이터베이스 접속 시 우선 키로 활용

	void Clear()
	{
		id = -1;
		max_score = 0;
		win_count = 0;
		lose_count = 0;
		nickname.clear();
		login_id.clear();
	}
};

struct DBResultUpdateScore : public DBResultDefault {
	DBResultUpdateScore() : DBResultDefault(DBOperationType::UPDATE_SCORE, true) {}

	int max_score;
};

struct DBResultUpdateMatchResult : public DBResultDefault {
	DBResultUpdateMatchResult() : DBResultDefault(DBOperationType::UPDATE_MATCH_RESULT, true) {}

	bool is_winner;
};

struct FriendInfo {
	int id;
	//int sess_gen; // db 요청 - 응답 사이에 재사용 판별을 위해 사용 -> 어차피 재사용 보장이 안돼서 탐색해서 찾아야겠다
	std::string nickname;
};

struct RankingInfo {
	int id;
	std::string nickname;
	int score;
};

struct DBResultAddFriend : public DBResultDefault { // 재조회하기는 싫으니까 그냥 닉네임을 받는걸로 하자
	// DB 요청 이후 두 세션은 존재하는지, 아닌지, 재로그인 했는지 알 수 없으므로 어차피 탐색해서 찾아야 함
	DBResultAddFriend() : DBResultDefault(DBOperationType::ADD_FRIEND, true) {}

	FriendInfo requester_info;
	FriendInfo accepter_info;
};

struct DBResultDeleteFriend : public DBResultDefault {
	DBResultDeleteFriend() : DBResultDefault(DBOperationType::DELETE_FRIEND, true) {}

	int requester_id;
	int target_id;
};

struct DBResultAddFriendRequest : public DBResultDefault {
	DBResultAddFriendRequest() : DBResultDefault(DBOperationType::ADD_FRIEND_REQUEST, true) {}

	FriendInfo requester_info;
	FriendInfo recver_info;
};

struct DBResultLoadFriendList : public DBResultDefault {
	DBResultLoadFriendList() : DBResultDefault(DBOperationType::LOAD_FRIEND_LIST, true) {}

	std::vector<FriendInfo> friend_list;
};

struct DBResultLoadRanking : public DBResultDefault {
	DBResultLoadRanking() : DBResultDefault(DBOperationType::LOAD_RANKING, true) {}

	std::vector<RankingInfo> rankings;
};
