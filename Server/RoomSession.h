#pragma once
#include <string>
#include <array>
#include "Tetris.h"
#include "define.h"
#include "Session.h"

class RoomSession
{
	Session* session = nullptr; // 상속으로 하면 세션을 받아올 수가 없음
	Tetris tetris;
	char send_buf[BUF_SIZE];
	int send_data_size = 0;
	// std::string user_name; // 방 생성할 때 만들도록 일단 하고, 나중에 회원가입 - DB 연동으로 session 클래스에 포함해보자.
	// char user_name[MAX_USER_NAME];
	Atomic<ROOM_USER_STATE> r_user_state;

	int tetromino_index = 0;

	int score = 0;

public:
	RoomSession();
	~RoomSession();

	Session* GetSession() const { return session; }
	ROOM_USER_STATE GetRoomUserState() const { return r_user_state.Load(); }
	Tetris& GetTetris() { return tetris; }
	int GetTetrominoIndex() const { return tetromino_index; }
	void AddTetrominoIndex() { ++tetromino_index; }
	int GetScore() const { return score; }

	void SetRoomUserState(ROOM_USER_STATE new_state) { return r_user_state.Store(new_state); }
	void SetScore(int val) { score = val; }	

	void InitSession(Session* s);
	void ClearSession();
	void ClearData();
	void AddToSendBuffer(const char* data, int data_size);
	void SendTickBatch(HANDLE iocp_handle);
	void ClearSendBuf();
};
