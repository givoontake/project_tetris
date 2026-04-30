#include <iostream>
#include <mutex>
#include "Session.h"

Session::Session()
{
	recv_over.SetOperationType(OP_TYPE::RECV);
}

void Session::InitSession(SOCKET new_socket)
{
	std::lock_guard<std::mutex> lock(sess_mutex);
	socket = new_socket;
	io_pending_count = 0;
	db_info.clear();
	friend_list.reserve(MAX_FRIENDS);
	room_index = -1;
	remain_data_size = 0;
	key.id = -1;
	recv_over.ex_over.key = key;
	life_state.Store(LIFE_STATE::ACTIVE);
	mode_state.Store(MODE_STATE::LOGIN);

	// ZeroMemory(&info, sizeof(info)); string은 제로메모리 하면 안됨,  string = 연산은 내부 필드 전체를 복사하는 연산이 아님
	//state = LOGIN;
}

bool Session::InitDBInfo(DBResultLogin* new_info)
{
	std::lock_guard<std::mutex> lock(sess_mutex);
	if (!(mode_state.Load() == MODE_STATE::LOGIN && life_state == LIFE_STATE::ACTIVE)) return false;
	db_info.id = new_info->id;
	key.id = new_info->id;
	db_info.login_id = new_info->login_id;
	db_info.nickname = new_info->nickname;
	db_info.lose_count = new_info->lose_count;
	db_info.win_count = new_info->win_count;
	db_info.max_score = new_info->max_score;
	mode_state.Store(MODE_STATE::LOBBY);
	return true;
}

void Session::SendPacket(char* packet, const HANDLE iocp_handle)
{
	if (!TryAddPending()) return;
	IOOverlapped* send_over = new IOOverlapped;
	send_over->SetOperationType(OP_TYPE::SEND);
	const auto packet_size = static_cast<int>(reinterpret_cast<const PacketHeader*>(packet)->size);
	memcpy(send_over->packet_buf, packet, packet_size);
	send_over->wsabuf.len = packet_size;
	send_over->ex_over.key = key;
	int ret = WSASend(socket, &send_over->wsabuf, 1, 0, 0, &send_over->ex_over.over, 0);
	if (ret == SOCKET_ERROR && WSAGetLastError() != WSA_IO_PENDING) {
		PostQueuedCompletionStatus(iocp_handle, 0, SESSION_IO_COMPLETION, reinterpret_cast<WSAOVERLAPPED*>(send_over));
		std::cerr << db_info.nickname << "Session::SendPacket() WSASend error\n";
	}
}


void Session::SendBoundPacket(char* packet_buf, int data_size, const HANDLE iocp_handle)
{
	if (data_size == 0) return;
	if (!TryAddPending()) return;
	IOOverlapped* send_over = new IOOverlapped;
	send_over->SetOperationType(OP_TYPE::SEND);
	memcpy(send_over->packet_buf, packet_buf, data_size);
	send_over->wsabuf.len = data_size;
	send_over->ex_over.key = key;
	int ret = WSASend(socket, &send_over->wsabuf, 1, 0, 0, &send_over->ex_over.over, 0);
	if (ret == SOCKET_ERROR && WSAGetLastError() != WSA_IO_PENDING) {
		PostQueuedCompletionStatus(iocp_handle, 0, SESSION_IO_COMPLETION, reinterpret_cast<WSAOVERLAPPED*>(send_over));
		std::cerr << db_info.nickname << " Session::SendBoundPacket() WSASend error\n";
	}
}


void Session::RecvPacket(const HANDLE iocp_handle)
{
	if (!TryAddPending()) return;
	DWORD recv_flag = 0;
	ZeroMemory(&recv_over.ex_over.over, sizeof(recv_over.ex_over.over)); // io 작업을 할 때마다 오버랩 구조체 초기화 필요(안정성)
	recv_over.ex_over.key = key;
	recv_over.wsabuf.len = BUF_SIZE - remain_data_size;
	recv_over.wsabuf.buf = recv_over.packet_buf + remain_data_size;
	int ret = WSARecv(socket, &recv_over.wsabuf, 1, 0, &recv_flag, &recv_over.ex_over.over, 0);
	if (ret == SOCKET_ERROR && WSAGetLastError() != WSA_IO_PENDING) {
		PostQueuedCompletionStatus(iocp_handle, 0, SESSION_IO_COMPLETION, reinterpret_cast<WSAOVERLAPPED*>(&recv_over));
		std::cerr << db_info.nickname << "Session::RecvPacket() WSARecv error\n";
	}
}

void Session::AddFriend(FriendInfo& new_friend)
{
	std::lock_guard<std::mutex> lock(sess_mutex);
	friend_list.emplace_back(new_friend);
}

void Session::DeleteFriend(int target_id)
{
	std::lock_guard<std::mutex> lock(sess_mutex);
	auto it = std::find_if(friend_list.begin(), friend_list.end(), [&target_id](const FriendInfo& friend_info) {
		return friend_info.id == target_id;
		});
	if (it != friend_list.end()) {
		friend_list.erase(it);
	}
}

void Session::InitFriendList(std::vector<FriendInfo>& db_friend_list)
{
	std::lock_guard<std::mutex> lock(sess_mutex);
	friend_list = std::move(db_friend_list);
}

DBResultLogin Session::UpdateMaxScore(int max_score)
{
	std::lock_guard<std::mutex> lock(sess_mutex);
	db_info.max_score = max_score;
	return db_info;
}

DBResultLogin Session::UpdateMatchRecord(bool is_winner)
{
	std::lock_guard<std::mutex> lock(sess_mutex);
	if (is_winner) ++db_info.win_count;
	else ++db_info.lose_count;
	return db_info;
}

void Session::SetIndex(int new_index)
{
	std::lock_guard<std::mutex> lock(sess_mutex);
	key.index = new_index;
}

void Session::AddDataSize(int new_data_size)
{
	std::lock_guard<std::mutex> lock(sess_mutex);
	remain_data_size += new_data_size;
}

// 현재 디스커넥팅 예외 케이스는 disconnect->방에서 deleteuser할 때 본인에게도 전송하는 로직이 있어서 그 부분이 방지. 세션 정리 단계이므로 넣는게 정배
bool Session::TryAddPending()
{
	std::lock_guard<std::mutex> lock(sess_mutex);
	if (life_state.Load() != LIFE_STATE::ACTIVE) return false;
	io_pending_count++;
	return true;	
}

void Session::ReducePending()
{
	std::lock_guard<std::mutex> lock(sess_mutex);
	--io_pending_count;
}

bool Session::IsDisconnectable()
{
	std::lock_guard<std::mutex> lock(sess_mutex);
	if (life_state.Load() == LIFE_STATE::DISCONNECT_PENDING && io_pending_count == 0) return true;
	return false;
}

bool Session::BeginDeactivate()
{
	std::lock_guard<std::mutex> lock(sess_mutex);
	if (life_state.Load() == LIFE_STATE::ACTIVE) {
		life_state.Store(LIFE_STATE::DISCONNECT_PENDING);
		return true;
	}
	return false;
}

bool Session::TryDeactivate()
{
	std::lock_guard<std::mutex> lock(sess_mutex);
	if (life_state.Load() == LIFE_STATE::DISCONNECT_PENDING && io_pending_count == 0) {
		life_state.Store(LIFE_STATE::DISCONNECTING);
		return true;
	}
	return false;
}

SOCKET Session::GetSocket() const
{
	std::lock_guard<std::mutex> lock(sess_mutex);
	return socket;
}

std::vector<FriendInfo> Session::GetFriendList() const
{
	std::lock_guard<std::mutex> lock(sess_mutex);
	return friend_list;
}

SessionKey Session::GetSessionKey() const
{
	std::lock_guard<std::mutex> lock(sess_mutex);
	return key;
}

int Session::GetRoomIndex() const
{
	std::lock_guard<std::mutex> lock(sess_mutex);
	return room_index;
}

RoomSnapShot Session::GetRoomSnapShot() const
{
	std::lock_guard<std::mutex> lock(sess_mutex);
	return { mode_state.Load(), room_index };
}

int Session::GetRemainDataSize() const
{
	std::lock_guard<std::mutex> lock(sess_mutex);
	return remain_data_size;
}

DBResultLogin Session::GetDBInfo() const
{
	std::lock_guard<std::mutex> lock(sess_mutex);
	return db_info;
}

void Session::StoreLifeState(LIFE_STATE new_state)
{
	std::lock_guard<std::mutex> lock(sess_mutex);
	life_state.Store(new_state);
}

void Session::StoreState(MODE_STATE new_state)
{
	std::lock_guard<std::mutex> lock(sess_mutex);
	if (life_state.Load() == LIFE_STATE::DISCONNECT_PENDING || life_state.Load() == LIFE_STATE::DISCONNECTING) return;
	mode_state.Store(new_state);
}

void Session::SetRoomSnapShot(MODE_STATE new_state, int new_room_index)
{
	std::lock_guard<std::mutex> lock(sess_mutex);
	if (life_state.Load() == LIFE_STATE::DISCONNECT_PENDING || life_state.Load() == LIFE_STATE::DISCONNECTING) return;
	room_index = new_room_index;
	mode_state.Store(new_state);
}

bool Session::TrySetRoomMode(int new_room_index)
{
	std::lock_guard<std::mutex> lock(sess_mutex);
	if (new_room_index < 0) return false;
	if (life_state.Load() != LIFE_STATE::ACTIVE) return false;
	if (mode_state.Load() != MODE_STATE::LOBBY) return false;
	room_index = new_room_index;
	mode_state.Store(MODE_STATE::ROOM);
	return true;
}

bool Session::TryChangeLifeState(LIFE_STATE expected, LIFE_STATE desired)
{
	std::lock_guard<std::mutex> lock(sess_mutex);
	return life_state.Compare_exchange_strong(expected, desired);
}

bool Session::TryChangeState(MODE_STATE expected, MODE_STATE desired)
{
	std::lock_guard<std::mutex> lock(sess_mutex);
	return mode_state.Compare_exchange_strong(expected, desired);
}

