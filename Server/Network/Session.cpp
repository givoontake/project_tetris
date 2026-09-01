#include <iostream>
#include <mutex>
#include "Session.h"

Session::Session()
{
	recv_over_.SetOperationType(OPType::RECV);
}

void Session::InitSession(SOCKET new_socket)
{
	std::lock_guard<std::mutex> lock(sess_mutex_);
	socket_ = new_socket;
	io_pending_count_ = 0;
	db_info_.Clear();
	friend_list_.reserve(MAX_FRIENDS);
	room_index_ = -1;
	remain_data_size_ = 0;
	key_.id = -1;
	recv_over_.ex_over.key = key_;
	life_state_.store(LifeState::ACTIVE);
	mode_state_.store(ModeState::LOGIN);

	// ZeroMemory(&info, sizeof(info)); string은 제로메모리 하면 안됨,  string = 연산은 내부 필드 전체를 복사하는 연산이 아님
	//state = LOGIN;
}

bool Session::InitDBInfo(DBResultLogin* new_info)
{
	std::lock_guard<std::mutex> lock(sess_mutex_);
	if (!(mode_state_.load() == ModeState::LOGIN && life_state_.load() == LifeState::ACTIVE)) return false;
	db_info_.id = new_info->id;
	key_.id = new_info->id;
	db_info_.login_id = new_info->login_id;
	db_info_.nickname = new_info->nickname;
	db_info_.lose_count = new_info->lose_count;
	db_info_.win_count = new_info->win_count;
	db_info_.max_score = new_info->max_score;
	mode_state_.store(ModeState::LOBBY);
	return true;
}

void Session::SendPacket(char* packet, const HANDLE iocp_handle)
{
	if (!TryAddPending()) return;
	IOOverlapped* send_over = new IOOverlapped;
	send_over->SetOperationType(OPType::SEND);
	const auto packet_size = static_cast<int>(reinterpret_cast<const PACKET_HEADER*>(packet)->size);
	memcpy(send_over->packet_buf, packet, packet_size);
	send_over->wsabuf.len = packet_size;
	send_over->ex_over.key = key_;
	int ret = WSASend(socket_, &send_over->wsabuf, 1, 0, 0, &send_over->ex_over.over, 0);
	if (ret == SOCKET_ERROR && WSAGetLastError() != WSA_IO_PENDING) {
		PostQueuedCompletionStatus(iocp_handle, 0, SESSION_IO_COMPLETION, reinterpret_cast<WSAOVERLAPPED*>(send_over));
		std::cerr << db_info_.nickname << "Session::SendPacket() WSASend error\n";
	}
}


void Session::SendBoundPacket(char* packet_buf, int data_size, const HANDLE iocp_handle)
{
	if (data_size == 0) return;
	if (!TryAddPending()) return;
	IOOverlapped* send_over = new IOOverlapped;
	send_over->SetOperationType(OPType::SEND);
	memcpy(send_over->packet_buf, packet_buf, data_size);
	send_over->wsabuf.len = data_size;
	send_over->ex_over.key = key_;
	int ret = WSASend(socket_, &send_over->wsabuf, 1, 0, 0, &send_over->ex_over.over, 0);
	if (ret == SOCKET_ERROR && WSAGetLastError() != WSA_IO_PENDING) {
		PostQueuedCompletionStatus(iocp_handle, 0, SESSION_IO_COMPLETION, reinterpret_cast<WSAOVERLAPPED*>(send_over));
		std::cerr << db_info_.nickname << " Session::SendBoundPacket() WSASend error\n";
	}
}


void Session::RecvPacket(const HANDLE iocp_handle)
{
	if (!TryAddPending()) return;
	DWORD recv_flag = 0;
	ZeroMemory(&recv_over_.ex_over.over, sizeof(recv_over_.ex_over.over)); // io 작업을 할 때마다 오버랩 구조체 초기화 필요(안정성)
	recv_over_.ex_over.key = key_;
	recv_over_.wsabuf.len = BUF_SIZE - remain_data_size_;
	recv_over_.wsabuf.buf = recv_over_.packet_buf + remain_data_size_;
	int ret = WSARecv(socket_, &recv_over_.wsabuf, 1, 0, &recv_flag, &recv_over_.ex_over.over, 0);
	if (ret == SOCKET_ERROR && WSAGetLastError() != WSA_IO_PENDING) {
		PostQueuedCompletionStatus(iocp_handle, 0, SESSION_IO_COMPLETION, reinterpret_cast<WSAOVERLAPPED*>(&recv_over_));
		std::cerr << db_info_.nickname << "Session::RecvPacket() WSARecv error\n";
	}
}

void Session::AddFriend(FriendInfo& new_friend)
{
	std::lock_guard<std::mutex> lock(sess_mutex_);
	friend_list_.emplace_back(new_friend);
}

void Session::DeleteFriend(int target_id)
{
	std::lock_guard<std::mutex> lock(sess_mutex_);
	auto it = std::find_if(friend_list_.begin(), friend_list_.end(), [&target_id](const FriendInfo& friend_info) {
		return friend_info.id == target_id;
		});
	if (it != friend_list_.end()) {
		friend_list_.erase(it);
	}
}

void Session::InitFriendList(std::vector<FriendInfo>& db_friend_list)
{
	std::lock_guard<std::mutex> lock(sess_mutex_);
	friend_list_ = std::move(db_friend_list);
}

DBResultLogin Session::UpdateMaxScore(int max_score)
{
	std::lock_guard<std::mutex> lock(sess_mutex_);
	db_info_.max_score = max_score;
	return db_info_;
}

DBResultLogin Session::UpdateMatchRecord(bool is_winner)
{
	std::lock_guard<std::mutex> lock(sess_mutex_);
	if (is_winner) ++db_info_.win_count;
	else ++db_info_.lose_count;
	return db_info_;
}

void Session::SetIndex(int new_index)
{
	std::lock_guard<std::mutex> lock(sess_mutex_);
	key_.index = new_index;
}

void Session::AddDataSize(int new_data_size)
{
	std::lock_guard<std::mutex> lock(sess_mutex_);
	remain_data_size_ += new_data_size;
}

// 현재 디스커넥팅 예외 케이스는 disconnect->방에서 deleteuser할 때 본인에게도 전송하는 로직이 있어서 그 부분이 방지. 세션 정리 단계이므로 넣는게 정배
bool Session::TryAddPending()
{
	std::lock_guard<std::mutex> lock(sess_mutex_);
	if (life_state_.load() != LifeState::ACTIVE) return false;
	io_pending_count_++;
	return true;	
}

void Session::ReducePending()
{
	std::lock_guard<std::mutex> lock(sess_mutex_);
	--io_pending_count_;
}

bool Session::IsDisconnectable()
{
	std::lock_guard<std::mutex> lock(sess_mutex_);
	if (life_state_.load() == LifeState::DISCONNECT_PENDING && io_pending_count_ == 0) return true;
	return false;
}

bool Session::BeginDeactivate()
{
	std::lock_guard<std::mutex> lock(sess_mutex_);
	if (life_state_.load() == LifeState::ACTIVE) {
		life_state_.store(LifeState::DISCONNECT_PENDING);
		return true;
	}
	return false;
}

bool Session::TryDeactivate()
{
	std::lock_guard<std::mutex> lock(sess_mutex_);
	if (life_state_.load() == LifeState::DISCONNECT_PENDING && io_pending_count_ == 0) {
		life_state_.store(LifeState::DISCONNECTING);
		return true;
	}
	return false;
}

SOCKET Session::GetSocket() const
{
	std::lock_guard<std::mutex> lock(sess_mutex_);
	return socket_;
}

std::vector<FriendInfo> Session::GetFriendList() const
{
	std::lock_guard<std::mutex> lock(sess_mutex_);
	return friend_list_;
}

SessionKey Session::GetSessionKey() const
{
	std::lock_guard<std::mutex> lock(sess_mutex_);
	return key_;
}

int Session::GetRoomIndex() const
{
	std::lock_guard<std::mutex> lock(sess_mutex_);
	return room_index_;
}

RoomSnapShot Session::GetRoomSnapShot() const
{
	std::lock_guard<std::mutex> lock(sess_mutex_);
	return { mode_state_.load(), room_index_ };
}

int Session::GetRemainDataSize() const
{
	std::lock_guard<std::mutex> lock(sess_mutex_);
	return remain_data_size_;
}

DBResultLogin Session::GetDBInfo() const
{
	std::lock_guard<std::mutex> lock(sess_mutex_);
	return db_info_;
}

void Session::StoreLifeState(LifeState new_state)
{
	std::lock_guard<std::mutex> lock(sess_mutex_);
	life_state_.store(new_state);
}

void Session::StoreState(ModeState new_state)
{
	std::lock_guard<std::mutex> lock(sess_mutex_);
	if (life_state_.load() == LifeState::DISCONNECT_PENDING || life_state_.load() == LifeState::DISCONNECTING) return;
	mode_state_.store(new_state);
}

void Session::SetRoomSnapShot(ModeState new_state, int new_room_index)
{
	std::lock_guard<std::mutex> lock(sess_mutex_);
	if (life_state_.load() == LifeState::DISCONNECT_PENDING || life_state_.load() == LifeState::DISCONNECTING) return;
	room_index_ = new_room_index;
	mode_state_.store(new_state);
}

bool Session::TrySetRoomMode(int new_room_index)
{
	std::lock_guard<std::mutex> lock(sess_mutex_);
	if (new_room_index < 0) return false;
	if (life_state_.load() != LifeState::ACTIVE) return false;
	if (mode_state_.load() != ModeState::LOBBY) return false;
	room_index_ = new_room_index;
	mode_state_.store(ModeState::ROOM);
	return true;
}

bool Session::TryChangeLifeState(LifeState expected, LifeState desired)
{
	std::lock_guard<std::mutex> lock(sess_mutex_);
	return life_state_.compare_exchange_strong(expected, desired);
}

bool Session::TryChangeState(ModeState expected, ModeState desired)
{
	std::lock_guard<std::mutex> lock(sess_mutex_);
	return mode_state_.compare_exchange_strong(expected, desired);
}

