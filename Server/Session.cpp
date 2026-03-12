#include <iostream>
#include "Session.h"
#include "packetType.h"

Session::Session()
{
	recv_over.SetOperationType(OP_TYPE::RECV);
}

void Session::InitSession(int new_index, int new_id, SOCKET new_socket)
{
	{
		// lock이 없다면 우연히 일부 필드만 초기화된 상태에서 작업이 일어날 수 있다.
		// 따라서 완전히 초기화 된 상태에서 접근하도록 한다.
		// 제너레이션은 이전 세션 요청이 즉시 재사용된 다음 세션에 잘못 영향을 주는 것을 막고
		// 뮤텍스는 즉시 재사용될 경우 일부 필드만 초기화된 상태에서 오류를 막는다.
		// 예시로 소켓만 초기화되고 이전 요청이 남아 들어오면, 제너레이션은 아직 변경되지 않았으므로 문제가 발생할 수 있다.
		std::lock_guard<std::mutex> lock(sess_mutex);
		socket = new_socket;
		recv_over.SetOperationId(new_id);
		info.clear();
		id = new_id;
		index = new_index;
		remain_data_size = 0; // 얘 기준으로 버퍼에 쓰니까 굳이 버퍼 자체를 초기화할 필요는 없어 보임.
		room_index = -1;
		disconnect_flag.Store(false);

		// ZeroMemory(&info, sizeof(info)); string은 제로메모리 하면 안됨,  string = 연산은 내부 필드 전체를 복사하는 연산이 아님
		//state = LOGIN;
	}

}

void Session::ClearSession()
{
	// 특정 작업 시 disconnect를 막아야 하므로 뮤텍스 추가
	// 예시로 db 작업 등록 중에는 disconnect 되면 안된다.
	{
		std::lock_guard<std::mutex> lock(sess_mutex);
		closesocket(socket);
		info.clear();
		recv_over.SetOperationId(-1);
		id = -1;
		index = -1;
		remain_data_size = 0;
		room_index = -1;
		state.Store(SESS_STATE::NONE);
	}
}

void Session::InitDBInfo(DBResultLogin* db_info)
{
	info.login_id = db_info->login_id;
	info.nickname = db_info->nickname;
	info.lose_count = db_info->lose_count;
	info.win_count = db_info->win_count;
	info.max_score = db_info->max_score;
}

void Session::SendPacket(char* packet, const HANDLE iocp_handle)
{
	if (state == SESS_STATE::NONE) return;
	IOOverlapped* send_over = new IOOverlapped;
	send_over->SetOperationType(OP_TYPE::SEND);
	send_over->SetOperationId(id);
	short packet_size = GetPacketSize(packet);
	memcpy(send_over->packet_buf, packet, packet_size);
	send_over->wsabuf.len = packet_size;
	int ret = WSASend(socket, &send_over->wsabuf, 1, 0, 0, &send_over->ex_over.over, 0);
	if (ret == SOCKET_ERROR && WSAGetLastError() != WSA_IO_PENDING) { // 이 작업이 실패했다는 것은 IOCP에 등록되지 않았다는 뜻, 그러나 이 실패는 DISCONNECT 사유에 해당
		PostQueuedCompletionStatus(iocp_handle, 0, index, reinterpret_cast<WSAOVERLAPPED*>(send_over)); // 따라서 IOCP에 직접 등록하고, 전송 바이트를 0으로 하여 IOCP 루프에서 DISCONNECT
		std::cout << id << " Session::SendPacket() WSASend error, PGCS\n";
	}
}

void Session::SendBoundPacket(char* packet_buf, int data_size, const HANDLE iocp_handle)
{
	if (state == SESS_STATE::NONE) return;
	IOOverlapped* send_over = new IOOverlapped;
	send_over->SetOperationType(OP_TYPE::SEND);
	send_over->SetOperationId(id);
	memcpy(send_over->packet_buf, packet_buf, data_size);
	send_over->wsabuf.len = data_size;
	int ret = WSASend(socket, &send_over->wsabuf, 1, 0, 0, &send_over->ex_over.over, 0);
	if (ret == SOCKET_ERROR && WSAGetLastError() != WSA_IO_PENDING) { // 이 작업이 실패했다는 것은 IOCP에 등록되지 않았다는 뜻, 그러나 이 실패는 DISCONNECT 사유에 해당
		PostQueuedCompletionStatus(iocp_handle, 0, index, reinterpret_cast<WSAOVERLAPPED*>(send_over)); // 따라서 IOCP에 직접 등록하고, 전송 바이트를 0으로 하여 IOCP 루프에서 DISCONNECT
		std::cout << id << " Session::SendBoundPacket() WSASend error, PGCS\n";
	}
}

void Session::RecvPacket(const HANDLE iocp_handle)
{
	if (state == SESS_STATE::NONE) return;
	DWORD recv_flag = 0;
	ZeroMemory(&recv_over.ex_over.over, sizeof(recv_over.ex_over.over)); // iocp 작업을 할 때마다 오버랩 구조체 초기화 필요(안정성)
	recv_over.wsabuf.len = BUF_SIZE - remain_data_size;
	recv_over.wsabuf.buf = recv_over.packet_buf + remain_data_size;
	int ret = WSARecv(socket, &recv_over.wsabuf, 1, 0, &recv_flag,
		&recv_over.ex_over.over, 0);
	if (ret == SOCKET_ERROR && WSAGetLastError() != WSA_IO_PENDING) {
		PostQueuedCompletionStatus(iocp_handle, 0, index, reinterpret_cast<WSAOVERLAPPED*>(&recv_over));
	}
}

short Session::GetPacketSize(char* packet)
{
	//switch (packet[2]) {
	//case S2C_TEST: {
	//	S2C_TEST_PACKET* p = reinterpret_cast<S2C_TEST_PACKET*>(packet);
	//	short packet_size;
	//	memcpy(&packet_size, packet, sizeof(packet_size));
	//	packet_size += p->message_size;
	//	return packet_size;
	//}
	//case C2S_TEST: {
	//	C2S_TEST_PACKET* p = reinterpret_cast<C2S_TEST_PACKET*>(packet);
	//	short packet_size;
	//	memcpy(&packet_size, packet, sizeof(packet_size));
	//	packet_size += p->message_size;
	//	return packet_size;
	//}
	//default: {
	//}
	//}

	short packet_size;
	memcpy(&packet_size, packet, sizeof(packet_size));
	return packet_size;
}

bool Session::TryChangeState(SESS_STATE expected, SESS_STATE desired)
{
	return state.Compare_exchange_strong(expected, desired);
}

bool Session::TryChangeDisconnectFlag(bool expected, bool desired)
{
	return disconnect_flag.Compare_exchange_strong(expected, desired);
}
