#include <iostream>
#include "Session.h"

Session::Session()
{
	recv_over.SetOperationType(OP_TYPE::RECV);
}

void Session::InitSession(int new_gen, SOCKET new_socket)
{
	// lock이 없다면 우연히 일부 필드만 초기화된 상태에서 작업이 일어날 수 있다.
	// 따라서 완전히 초기화 된 상태에서 접근하도록 한다.
	// 제너레이션은 이전 세션 요청이 즉시 재사용된 다음 세션에 잘못 영향을 주는 것을 막고
	// 뮤텍스는 즉시 재사용될 경우 일부 필드만 초기화된 상태에서 오류를 막는다.
	// 예시로 소켓만 초기화되고 이전 요청이 남아 들어오면, 제너레이션은 아직 변경되지 않았으므로 문제가 발생할 수 있다.
	std::lock_guard<std::mutex> lock(sess_mutex);
	socket = new_socket;
	db_info.clear();
	friend_list.reserve(MAX_FRIENDS);
	key.gen = new_gen;
	remain_data_size = 0;
	disconnect_flag.Store(false);
	recv_over.ex_over.key = key;

	// ZeroMemory(&info, sizeof(info)); string은 제로메모리 하면 안됨,  string = 연산은 내부 필드 전체를 복사하는 연산이 아님
	//state = LOGIN;
}

void Session::ClearSession()
{
	// 특정 작업 시 disconnect를 막아야 하므로 뮤텍스 추가
	// 예시로 db 작업 등록 중에는 disconnect 되면 안된다.
	std::lock_guard<std::mutex> lock(sess_mutex);
	closesocket(socket);
	db_info.clear();
	friend_list.clear();
	key.gen = -1;
	// tcp에서 패킷을 나누어 보낼 때 비정상 종료되면 일부만 보내고 끝날 수도 있다고 한다
	// 따라서 remain_data_size는 항상 초기화가 필요하다
	remain_data_size = 0;
	recv_over.ex_over.key = key;
	state.Store(SESS_STATE::NONE);
}

void Session::InitDBInfo(DBResultLogin* new_info)
{
	db_info.id = new_info->id;
	db_info.login_id = new_info->login_id;
	db_info.nickname = new_info->nickname;
	db_info.lose_count = new_info->lose_count;
	db_info.win_count = new_info->win_count;
	db_info.max_score = new_info->max_score;
}

// IOKey reqeust_sess_key는 재사용 여부를 거르기 위한 장치다. 외부에서 받은 것과 현재 키를 비교한다.
// 세션 수명에 대해 신뢰가 가능한 곳에서는 키가 없는 함수를 호출
// GQCS에서 넘어온 키는 작업을 동록한 세션의 명확한 식별자이다. 만약 다르다면 이전 사용자의 작업이므로 무시해야 한다.
void Session::SendPacket(char* packet, const HANDLE iocp_handle)
{
	IOOverlapped* send_over = new IOOverlapped;
	send_over->SetOperationType(OP_TYPE::SEND);
	const auto packet_size = static_cast<int>(reinterpret_cast<const PacketHeader*>(packet)->size);
	memcpy(send_over->packet_buf, packet, packet_size);
	send_over->wsabuf.len = packet_size;
	send_over->ex_over.key = key;
	int ret = WSASend(socket, &send_over->wsabuf, 1, 0, 0, &send_over->ex_over.over, 0);
	if (ret == SOCKET_ERROR && WSAGetLastError() != WSA_IO_PENDING) {
		PostQueuedCompletionStatus(iocp_handle, 0, SESSION_IO_COMPLETION, reinterpret_cast<WSAOVERLAPPED*>(send_over));
		std::cerr << key.gen << " Session::SendPacket() WSASend error\n";
	}
}

void Session::SendPacket(int request_gen, char* packet, const HANDLE iocp_handle)
{
	IOOverlapped* send_over = new IOOverlapped;
	send_over->SetOperationType(OP_TYPE::SEND);
	const auto packet_size = static_cast<int>(reinterpret_cast<const PacketHeader*>(packet)->size);
	memcpy(send_over->packet_buf, packet, packet_size);
	send_over->wsabuf.len = packet_size;
	{
		std::lock_guard<std::mutex> lock(sess_mutex);
		if (key.gen == request_gen) {
			send_over->ex_over.key = key;
			// 같다면 재사용되지 않았다는 것이고 중간에 NONE이 된 적이 없다는 말이므로 굳이 상태 비교는 필요없다.
			int ret = WSASend(socket, &send_over->wsabuf, 1, 0, 0, &send_over->ex_over.over, 0);
			if (ret == SOCKET_ERROR && WSAGetLastError() != WSA_IO_PENDING) {
				PostQueuedCompletionStatus(iocp_handle, 0, SESSION_IO_COMPLETION, reinterpret_cast<WSAOVERLAPPED*>(send_over));
				std::cerr << key.gen << " Session::SendPacket() WSASend error\n";
			}
		}
		else {
			std::cerr << "Session::SendPacket, Session index[" << key.index << "]slot has been reused." << std::endl;
			delete send_over;
			return;
		}
	}
}

void Session::SendBoundPacket(char* packet_buf, int data_size, const HANDLE iocp_handle)
{
	IOOverlapped* send_over = new IOOverlapped;
	send_over->SetOperationType(OP_TYPE::SEND);
	memcpy(send_over->packet_buf, packet_buf, data_size);
	send_over->wsabuf.len = data_size;
	send_over->ex_over.key = key;
	int ret = WSASend(socket, &send_over->wsabuf, 1, 0, 0, &send_over->ex_over.over, 0);
	if (ret == SOCKET_ERROR && WSAGetLastError() != WSA_IO_PENDING) {
		PostQueuedCompletionStatus(iocp_handle, 0, SESSION_IO_COMPLETION, reinterpret_cast<WSAOVERLAPPED*>(send_over));
		std::cerr << key.gen << " Session::SendBoundPacket() WSASend error\n";
	}
}

void Session::SendBoundPacket(int request_gen, char* packet_buf, int data_size, const HANDLE iocp_handle)
{
	if (data_size == 0) return;
	IOOverlapped* send_over = new IOOverlapped;
	send_over->SetOperationType(OP_TYPE::SEND);
	memcpy(send_over->packet_buf, packet_buf, data_size);
	send_over->wsabuf.len = data_size;
	{
		std::lock_guard<std::mutex> lock(sess_mutex);
		if (key.gen == request_gen) {
			send_over->ex_over.key = key;
			int ret = WSASend(socket, &send_over->wsabuf, 1, 0, 0, &send_over->ex_over.over, 0);
			if (ret == SOCKET_ERROR && WSAGetLastError() != WSA_IO_PENDING) {
				PostQueuedCompletionStatus(iocp_handle, 0, SESSION_IO_COMPLETION, reinterpret_cast<WSAOVERLAPPED*>(send_over));
				std::cerr << key.gen << " Session::SendPacket() WSASend error\n";
			}
		}
		else {
			std::cerr << "Session::SendPacket, Session index[" << key.index << "]slot has been reused." << std::endl;
			delete send_over;
			return;
		}
	}
}

void Session::RecvPacket(int request_gen, const HANDLE iocp_handle)
{
	DWORD recv_flag = 0;
	ZeroMemory(&recv_over.ex_over.over, sizeof(recv_over.ex_over.over)); // iocp 작업을 할 때마다 오버랩 구조체 초기화 필요(안정성)
	{
		std::lock_guard<std::mutex> lock(sess_mutex);
		if (state == SESS_STATE::NONE) return;
		if (key.gen == request_gen) {
			recv_over.ex_over.key = key;
			recv_over.wsabuf.len = BUF_SIZE - remain_data_size;
			recv_over.wsabuf.buf = recv_over.packet_buf + remain_data_size;
			int ret = WSARecv(socket, &recv_over.wsabuf, 1, 0, &recv_flag, &recv_over.ex_over.over, 0);
			if (ret == SOCKET_ERROR && WSAGetLastError() != WSA_IO_PENDING) {
				PostQueuedCompletionStatus(iocp_handle, 0, SESSION_IO_COMPLETION, reinterpret_cast<WSAOVERLAPPED*>(&recv_over));
				std::cerr << key.gen << " Session::RecvPacket() WSARecv error\n";
			}
		}
		else {
			std::cerr << "Session::RecvPacket, Session index[" << key.index << "]slot has been reused." << std::endl;
			return;
		}
	}
}

void Session::AddFriend(FriendInfo& new_friend)
{
	friend_list.emplace_back(new_friend);
}

void Session::DeleteFriend(int target_id)
{
	auto it = std::find_if(friend_list.begin(), friend_list.end(), [&target_id](const FriendInfo& friend_info) {
		return friend_info.id == target_id;
		});
	if (it != friend_list.end()) {
		friend_list.erase(it);
	}
}

void Session::InitFriendList(std::vector<FriendInfo>& db_friend_list)
{
	friend_list = std::move(db_friend_list);
}

bool Session::TryChangeState(SESS_STATE expected, SESS_STATE desired)
{
	return state.Compare_exchange_strong(expected, desired);
}

bool Session::TryChangeDisconnectFlag(bool expected, bool desired)
{
	return disconnect_flag.Compare_exchange_strong(expected, desired);
}
