#include <iostream>
#include "Session.h"

Session::Session()
{
	recv_over.SetExOverlapped(RECV);
}

void Session::SendPacket(char* packet, HANDLE iocp_handle)
{
	if (s_state == NONE) return;
	ExOverlapped* send_over = new ExOverlapped;
	send_over->SetExOverlapped(SEND);
	short packet_size = GetPacketSize(packet);
	memcpy(send_over->packet_buf, packet, packet_size);
	send_over->wsabuf.len = packet_size;
	int ret = WSASend(socket, &send_over->wsabuf, 1, 0, 0, &send_over->over, 0);
	if (ret == SOCKET_ERROR && WSAGetLastError() != WSA_IO_PENDING) {
		PostQueuedCompletionStatus(iocp_handle, 0, index, reinterpret_cast<WSAOVERLAPPED*>(send_over));
	}
}

void Session::RecvPacket(HANDLE iocp_handle)
{
	if (s_state == NONE) return;
	int sign_count = 0;
	DWORD recv_flag = 0;
	ZeroMemory(&recv_over.over, sizeof(recv_over.over)); // iocp 작업을 할 때마다 오버랩 구조체 초기화 필요(안정성)
	recv_over.wsabuf.len = BUF_SIZE - remain_data_size;
	recv_over.wsabuf.buf = recv_over.packet_buf + remain_data_size;
	int ret = WSARecv(socket, &recv_over.wsabuf, 1, 0, &recv_flag,
		&recv_over.over, 0);
	if (ret == SOCKET_ERROR && WSAGetLastError() != WSA_IO_PENDING) {
		PostQueuedCompletionStatus(iocp_handle, 0, index, reinterpret_cast<WSAOVERLAPPED*>(&recv_over));
	}
}

short Session::GetPacketSize(char* packet)
{
	short packet_size;
	memcpy(&packet_size, packet, sizeof(packet_size));
	return packet_size;
}

void Session::InitSession()
{
	index = -1;
	id = -1;
	remain_data_size = 0;
	last_time = -1;
	last_send_time = -1;
}

void Session::ClearSession()
{
	index = -1;
	id = -1;
	remain_data_size = 0;
	//last_time = -1;
	//last_send_time = -1;

	//s_state = NONE;
}

bool Session::SetState(SESSION_STATE expected, SESSION_STATE desired)
{
	if (desired != expected) {
		if (s_state.compare_exchange_strong(expected, desired)) return true;
		else return false;
	}

	// 같으면 원하는 상태이므로 true 반환
	return true;
}

void Session::SetState(SESSION_STATE desired)
{
	s_state = desired;
}


