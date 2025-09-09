#pragma once
#include <string>
#include "Tetris.h"
#include "define.h"
#include "Session.h"

class RoomSession
{
	Session* session = nullptr; // 상속으로 하면 세션을 받아올 수가 없음
	Tetris tetris;
	// std::string user_name; // 방 생성할 때 만들도록 일단 하고, 나중에 회원가입 - DB 연동으로 session 클래스에 포함해보자.
	// char user_name[MAX_USER_NAME];
	bool is_ready = false;
	Atomic<bool> in_use = false; // 룸에서 해당 배열 인덱스가 사용 중인지를 판별하기 위한 변수

	long long timer;

public:
	RoomSession();
	~RoomSession();

	Session* GetSession() const { return session; }
	bool GetInUse() const { return in_use.GetSelf(); }
	bool GetIsReady() const { return is_ready; }
	Tetris& GetTetris() { return tetris; }

	void SetIsReady(bool param);
	bool SetUse(bool expected, bool desired);

	void InitSession(Session* s);
	void ClearSession();
};

