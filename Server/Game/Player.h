#pragma once
#include <atomic>
#include <string>
#include <memory>
#include "types.h"
#include "Tetris.h"
#include "define_packets.h"
#include "Session.h"

class Player
{
	WP<Session> session_;
	Tetris tetris_;
	char send_buffer_[BUF_SIZE];
	int send_data_size_ = 0;
	// std::string player_name; // 방 생성할 때 만들도록 일단 하고, 나중에 회원가입 - DB 연동으로 session 클래스에 포함해보자.
	// char player_name[MAX_PLAYER_NAME_SIZE];
	std::atomic<RoomPlayerState> room_player_state_;
	std::atomic<RoomPlayerState> prev_room_player_state_; // 게임 오버시 무승부 체크용

	int tetromino_index_ = 0;

	int score_ = 0;
	int combo_ = 0;

public:
	Player();
	~Player();

	SP<Session> GetSession() const { return session_.lock(); }
	RoomPlayerState GetRoomPlayerState() const { return room_player_state_.load(); }
	RoomPlayerState GetPrevRoomPlayerState() const { return prev_room_player_state_.load(); }
	Tetris& GetTetris() { return tetris_; }
	char* GetSendBuffer() { return send_buffer_; }
	int GetSendDataSize() const { return send_data_size_; }
	int GetTetrominoIndex() const { return tetromino_index_; }
	void IncrementTetrominoIndex() { ++tetromino_index_; }
	int GetScore() const { return score_; }
	int GetCombo() const { return combo_; }

	void SetRoomPlayerState(RoomPlayerState new_state) { return room_player_state_.store(new_state); }
	void SetPrevRoomPlayerState(RoomPlayerState new_state) { return prev_room_player_state_.store(new_state); }
	void AddScore(int val) { score_ += val; }
	void ResetCombo() { combo_ = 0; }
	void AddCombo() { ++combo_; }

	bool InitPlayer(const SP<Session>& session, int room_index);
	void ClearPlayer();
	void ResetGameData();
	bool AddToSendBuffer(const char* data, int data_size);
	//void SendTickData(HANDLE iocp_handle);
	void ClearSendBuffer();
};
