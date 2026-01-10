#include <iostream>
#include "Session.h"
#include "packetType.h"

Session::Session()
{
	recv_over.SetOperationType(RECV);
}

void Session::InitSession(int new_index, int new_id, SOCKET new_socket)
{
	id = new_id;
	recv_over.SetOperationId(new_id);
	index = new_index;
	socket = new_socket;
	remain_data_size = 0; // 얘 기준으로 버퍼에 쓰니까 굳이 버퍼 자체를 초기화할 필요는 없어 보임.
	room_index = -1;
	//state = LOGIN;
}

void Session::InitDBInfo(DBInfo* db_info)
{
	memcpy(&info, db_info, sizeof(info));
}

void Session::SendPacket(char* packet, const HANDLE iocp_handle)
{
	if (state == NONE) return;
	IOOverlapped* send_over = new IOOverlapped;
	send_over->SetOperationType(SEND);
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

void Session::SendBoundPacket(char* packet, int data_size, const HANDLE iocp_handle)
{
	if (state == NONE) return;
	IOOverlapped* send_over = new IOOverlapped;
	send_over->SetOperationType(SEND);
	send_over->SetOperationId(id);
	memcpy(send_over->packet_buf, packet, data_size);
	send_over->wsabuf.len = data_size;
	int ret = WSASend(socket, &send_over->wsabuf, 1, 0, 0, &send_over->ex_over.over, 0);
	if (ret == SOCKET_ERROR && WSAGetLastError() != WSA_IO_PENDING) { // 이 작업이 실패했다는 것은 IOCP에 등록되지 않았다는 뜻, 그러나 이 실패는 DISCONNECT 사유에 해당
		PostQueuedCompletionStatus(iocp_handle, 0, index, reinterpret_cast<WSAOVERLAPPED*>(send_over)); // 따라서 IOCP에 직접 등록하고, 전송 바이트를 0으로 하여 IOCP 루프에서 DISCONNECT
		std::cout << id << " Session::SendBoundPacket() WSASend error, PGCS\n";
	}
}

void Session::RecvPacket(const HANDLE iocp_handle)
{
	if (state == NONE) return;
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

bool Session::SetState(USER_STATE expected, USER_STATE desired)
{
	// false -> true
	if (desired != expected) {
		if (state.Compare_exchange_strong(expected, desired)) return true;
		else return false;
	}

	// 같으면 원하는 상태이므로 true 반환
	return true;
}
