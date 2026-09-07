#pragma once
#include <WinSock2.h>
#include <MSWSock.h>
#include <array>
#include <atomic>
#include <cstdint>
#include <optional>
#include "ExOverlapped.h"

enum class SessionState
{
	NONE,
	CONNECTING,
	LOGIN,
	LOBBY,
	ENTER_ROOM,
	ROOM,
	PLAYING,
	DISCONNECTING
};

class Session
{
	SOCKET socket_ = INVALID_SOCKET;
	ExOverlapped recv_over_;
	int index_ = -1;
	int player_id_ = -1;
	int remaining_data_size_ = 0;
	std::atomic<SessionState> state_{ SessionState::NONE };
	std::atomic<long long> last_send_time_{ -1 };
	std::array<long long, MOVE_TIME_QUEUE_SIZE> move_send_times_{};
	std::atomic<std::uint32_t> move_time_write_index_{ 0 };
	std::atomic<std::uint32_t> move_time_read_index_{ 0 };
	std::atomic<int> next_move_type_{ 0 };
	std::atomic<int> ready_player_count_{ 0 };

public:
	Session();

	bool SendPacket(const char* packet, HANDLE iocp_handle);
	void RecvPacket(HANDLE iocp_handle);
	void InitSession();
	void ClearSession();
	bool PushMoveSendTime(long long send_time);
	std::optional<long long> PopMoveSendTime();

	SOCKET GetSocket() const { return socket_; }
	ExOverlapped& GetRecvOverlapped() { return recv_over_; }
	int GetIndex() const { return index_; }
	int GetPlayerID() const { return player_id_; }
	int GetRemainingDataSize() const { return remaining_data_size_; }
	SessionState GetState() const { return state_.load(); }
	long long GetLastSendTime() const { return last_send_time_.load(); }
	int GetNextMoveType() { return next_move_type_.fetch_xor(1); }
	int IncrementReadyPlayerCount() { return ready_player_count_.fetch_add(1) + 1; }

	void SetSocket(SOCKET new_socket) { socket_ = new_socket; }
	void SetIndex(int new_index) { index_ = new_index; }
	void SetPlayerID(int new_player_id) { player_id_ = new_player_id; }
	void AdjustRemainingDataSize(int size) { remaining_data_size_ += size; }
	void SetLastSendTime(long long send_time) { last_send_time_.store(send_time); }
	void ResetReadyPlayerCount() { ready_player_count_.store(0); }
	bool TrySetLastSendTime(long long& expected, long long desired) { return last_send_time_.compare_exchange_strong(expected, desired); }
	bool TrySetState(SessionState expected, SessionState desired);
	void SetState(SessionState desired);
};
