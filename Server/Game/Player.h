#pragma once
#include <atomic>
#include <string>
#include "Tetris.h"
#include "types.h"
#include "settings.h"
#include "ExOverlapped.h"

class Session;

class Player
{
	SessionKey session_key_; // 세션 정리 후에도 방 슬롯을 식별하고, 재사용된 세션 인덱스의 오삭제를 막는다.
	std::atomic<ActiveEntryState> active_state_{ ActiveEntryState::EMPTY };
	Tetris tetris_;
	char send_buffer_[BUF_SIZE];
	int send_data_size_ = 0;
	std::atomic<RoomPlayerState> room_player_state_;
	std::atomic<RoomPlayerState> prev_room_player_state_; // 게임 오버시 무승부 체크용

	int tetromino_index_ = 0;

	int score_ = 0;
	int combo_ = 0;

public:
	Player();
	~Player();

	SessionKey GetSessionKey() const { return session_key_; }
	bool HasSession() const { return active_state_.load() != ActiveEntryState::EMPTY; }
	bool IsActive() const { return active_state_.load() == ActiveEntryState::ACTIVE; }
	ActiveEntryState GetActiveState() const { return active_state_.load(); }
	bool MatchesSessionKey(SessionKey session_key) const;
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

	bool InitPlayer(Session& session, SessionKey session_key, int room_index);
	bool Activate(SessionKey session_key);
	bool TrySetPending(SessionKey session_key);
	void ClearPlayer();
	void ResetGameData();
	bool AddToSendBuffer(const char* data, int data_size);
	void ClearSendBuffer();
};
