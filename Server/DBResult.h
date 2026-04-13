#pragma once
#include <string>
#include <vector>
#include "define.h"

struct DBResultDefault {
	// 유니크 포인터용 껍데기 구조체
	// void*는 삭제가 불가능하므로, 삭제가 가능한 껍데기 구조체로 관리

	// 부모 클래스에 1번 선언된 가상함수는 자식 함수에서 모두 가상함수(소멸자, 일반 함수 모두 마찬가지, virtual / override를 쓰는 것은 가상함수 정의 의도를 표현하는 것, 자식에서 안써도 가상함수로 잘 동작한다.)
	virtual ~DBResultDefault() = default;
};

struct DBResultLogin : public DBResultDefault {
	int id;
	int max_score;
	int win_count;
	int lose_count;
	std::string nickname;
	std::string login_id; // 데이터베이스 접속 시 우선 키로 활용

	void clear()
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
	int max_score;
};

struct DBResultUpdateMatchResult : public DBResultDefault {
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
	FriendInfo requester_info;
	FriendInfo accepter_info;
};

struct DBResultDeleteFriend : public DBResultDefault {
	int requester_id;
	int target_id;
};

struct DBResultAddFriendRequest : public DBResultDefault {
	FriendInfo requester_info;
	FriendInfo recver_info;
};

struct DBResultLoadFriendList : public DBResultDefault {
	std::vector<FriendInfo> friend_list;
};

struct DBResultLoadRanking : public DBResultDefault {
	std::vector<RankingInfo> rankings;
};
