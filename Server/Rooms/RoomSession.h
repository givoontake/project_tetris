#pragma once
#include <atomic>
#include <string>
#include <memory>
#include "types.h"
#include "Tetris.h"
#include "define_packets.h"
#include "Session.h"

class RoomSession
{
	WP<Session> session_;
	Tetris tetris_;
	char send_buf_[BUF_SIZE];
	int send_data_size_ = 0;
	// std::string user_name; // 방 생성할 때 만들도록 일단 하고, 나중에 회원가입 - DB 연동으로 session 클래스에 포함해보자.
	// char user_name[MAX_USER_NAME];
	std::atomic<RoomUserState> r_user_state_;
	std::atomic<RoomUserState> prev_r_user_state_; // 게임 오버시 무승부 체크용

	int tetromino_index_ = 0;

	int score_ = 0;
	int combo_ = 0;

public:
	RoomSession();
	~RoomSession();

	SP<Session> GetSession() const { return session_.lock(); }
	RoomUserState GetRoomUserState() const { return r_user_state_.load(); }
	RoomUserState GetPrevRoomUserState() const { return prev_r_user_state_.load(); }
	Tetris& GetTetris() { return tetris_; }
	char* GetSendBuf() { return send_buf_; }
	int GetSendDataSize() const { return send_data_size_; }
	int GetTetrominoIndex() const { return tetromino_index_; }
	void AddTetrominoIndex() { ++tetromino_index_; }
	int GetScore() const { return score_; }
	int GetCombo() const { return combo_; }

	void SetRoomUserState(RoomUserState new_state) { return r_user_state_.store(new_state); }
	void SetPrevRoomUserState(RoomUserState new_state) { return prev_r_user_state_.store(new_state); }
	void AddScore(int val) { score_ += val; }
	void ResetCombo() { combo_ = 0; }
	void AddCombo() { ++combo_; }

	bool InitRoomSession(const SP<Session>& s, int room_index);
	void ClearRoomSession();
	void ClearData();
	bool AddToSendBuffer(const char* data, int data_size);
	//void SendTickData(HANDLE iocp_handle);
	void ClearSendBuf();
};
