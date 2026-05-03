#pragma once
#include <string>
#include <memory>
#include "Types.h"
#include "Tetris.h"
#include "define_packets.h"
#include "Session.h"

class RoomSession
{
	WP<Session> session; 
	Tetris tetris;
	char tick_buf[BUF_SIZE];
	std::unique_ptr<char[]> send_buf;
	int send_buf_size = 0;
	int tick_data_size = 0;
	int send_data_size = 0;
	// std::string user_name; // 방 생성할 때 만들도록 일단 하고, 나중에 회원가입 - DB 연동으로 session 클래스에 포함해보자.
	// char user_name[MAX_USER_NAME];
	Atomic<ROOM_USER_STATE> r_user_state;
	Atomic<ROOM_USER_STATE> prev_r_user_state; // 게임 오버시 무승부 체크용

	int tetromino_index = 0;

	int score = 0;
	int combo = 0;

public:
	RoomSession(int max_user = 1);
	RoomSession(const RoomSession&) = delete;
	RoomSession& operator=(const RoomSession&) = delete;
	RoomSession(RoomSession&& other) noexcept = default;
	RoomSession& operator=(RoomSession&& other) noexcept = default;

	SP<Session> GetSession() const { return session.lock(); }
	ROOM_USER_STATE GetRoomUserState() const { return r_user_state.Load(); }
	ROOM_USER_STATE GetPrevRoomUserState() const { return prev_r_user_state.Load(); }
	Tetris& GetTetris() { return tetris; }
	char* GetTickBuf() { return tick_buf; }
	int GetTickDataSize() const { return tick_data_size; }
	char* GetSendBuf() { return send_buf.get(); }
	int GetSendDataSize() const { return send_data_size; }
	int GetTetrominoIndex() const { return tetromino_index; }
	void AddTetrominoIndex() { ++tetromino_index; }
	int GetScore() const { return score; }
	int GetCombo() const { return combo; }

	void SetRoomUserState(ROOM_USER_STATE new_state) { return r_user_state.Store(new_state); }
	void SetPrevRoomUserState(ROOM_USER_STATE new_state) { return prev_r_user_state.Store(new_state); }
	void AddScore(int val) { score += val; }	
	void ResetCombo() { combo = 0; }
	void AddCombo() { ++combo; }

	bool InitRoomSession(const SP<Session>& s, int room_index);
	void ClearRoomSession();
	void ClearData();
	bool AddToTickBuffer(const char* data, int data_size);
	bool AddToSendBuffer(const char* data, int data_size);
	//void SendTickData(HANDLE iocp_handle);
	void ClearTickBuf();
	void ClearSendBuf();
	void ClearBuffers();
};
