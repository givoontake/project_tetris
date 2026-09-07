#include <cstring>
#include "Session.h"

Session::Session()
{
	recv_over_.SetOperationType(OperationType::RECV);
}

bool Session::SendPacket(const char* packet, HANDLE iocp_handle)
{
	if (!packet || state_.load() == SessionState::NONE || state_.load() == SessionState::DISCONNECTING) return false;
	const std::uint16_t packet_size = reinterpret_cast<const PacketHeader*>(packet)->size;
	if (packet_size < sizeof(PacketHeader) || packet_size > BUF_SIZE) return false;

	ExOverlapped* send_over = new ExOverlapped;
	send_over->SetOperationType(OperationType::SEND);
	memcpy(send_over->packet_buffer, packet, packet_size);
	send_over->wsabuf.len = packet_size;
	const int result = WSASend(socket_, &send_over->wsabuf, 1, nullptr, 0, &send_over->over, nullptr);
	if (result == SOCKET_ERROR && WSAGetLastError() != WSA_IO_PENDING) {
		PostQueuedCompletionStatus(iocp_handle, 0, index_, reinterpret_cast<WSAOVERLAPPED*>(send_over));
		return false;
	}
	return true;
}

void Session::RecvPacket(HANDLE iocp_handle)
{
	if (state_.load() == SessionState::NONE || state_.load() == SessionState::DISCONNECTING) return;
	DWORD recv_flag = 0;
	ZeroMemory(&recv_over_.over, sizeof(recv_over_.over));
	recv_over_.SetOperationType(OperationType::RECV);
	recv_over_.wsabuf.len = BUF_SIZE - remaining_data_size_;
	recv_over_.wsabuf.buf = recv_over_.packet_buffer + remaining_data_size_;
	const int result = WSARecv(socket_, &recv_over_.wsabuf, 1, nullptr, &recv_flag, &recv_over_.over, nullptr);
	if (result == SOCKET_ERROR && WSAGetLastError() != WSA_IO_PENDING) {
		PostQueuedCompletionStatus(iocp_handle, 0, index_, reinterpret_cast<WSAOVERLAPPED*>(&recv_over_));
	}
}

void Session::InitSession()
{
	player_id_ = -1;
	remaining_data_size_ = 0;
	last_send_time_.store(-1);
	move_time_write_index_.store(0);
	move_time_read_index_.store(0);
	next_move_type_.store(0);
	ready_player_count_.store(0);
}

void Session::ClearSession()
{
	index_ = -1;
	player_id_ = -1;
	remaining_data_size_ = 0;
	last_send_time_.store(-1);
	move_time_write_index_.store(0);
	move_time_read_index_.store(0);
	next_move_type_.store(0);
	ready_player_count_.store(0);
}

bool Session::PushMoveSendTime(long long send_time)
{
	const std::uint32_t write_index = move_time_write_index_.load();
	const std::uint32_t read_index = move_time_read_index_.load();
	if (write_index - read_index >= MOVE_TIME_QUEUE_SIZE) return false;
	move_send_times_[write_index % MOVE_TIME_QUEUE_SIZE] = send_time;
	move_time_write_index_.store(write_index + 1);
	return true;
}

std::optional<long long> Session::PopMoveSendTime()
{
	const std::uint32_t read_index = move_time_read_index_.load();
	const std::uint32_t write_index = move_time_write_index_.load();
	if (read_index == write_index) return std::nullopt;
	const long long send_time = move_send_times_[read_index % MOVE_TIME_QUEUE_SIZE];
	move_time_read_index_.store(read_index + 1);
	return send_time;
}

bool Session::TrySetState(SessionState expected, SessionState desired)
{
	return state_.compare_exchange_strong(expected, desired);
}

void Session::SetState(SessionState desired)
{
	state_.store(desired);
}
