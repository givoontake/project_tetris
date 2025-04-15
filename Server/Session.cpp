#include <iostream>
#include "Session.h"

Session::Session()
{
	recv_over.SetExOverlapped(RECV);
}

void Session::SendPacket(char* packet)
{
	ExOvelapped* send_over = new ExOvelapped;
	send_over->SetExOverlapped(SEND);
	memcpy(send_over->packet_buf, packet, packet[0]);
	int ret = WSASend(socket, &send_over->wsabuf, 1, 0, 0, &send_over->over, 0);
	if (ret == SOCKET_ERROR && WSAGetLastError() != WSA_IO_PENDING) {
		// 이건 GQCS로 가지 않음 → 직접 처리해야 함
		std::cout << "client[" << id << "]" << "WSASend() IOCP Sign Up Fail\n";
		delete send_over;
	}
}

void Session::RecvPacket()
{
	DWORD recv_flag = 0;
	ZeroMemory(&recv_over.over, sizeof(recv_over.over)); // iocp 작업을 할 때마다 오버랩 구조체 초기화 필요(안정성)
	recv_over.wsabuf.len = BUF_SIZE - remain_data_size;
	recv_over.wsabuf.buf = recv_over.packet_buf + remain_data_size;
	int ret = WSARecv(socket, &recv_over.wsabuf, 1, 0, &recv_flag,
		&recv_over.over, 0);
	if (ret == SOCKET_ERROR && WSAGetLastError() != WSA_IO_PENDING) {
		// 이건 GQCS로 가지 않음 → 직접 처리해야 함
		std::cout << "client[" << id << "]" << "WSARecv() IOCP Sign Up Fail\n";
	}
}
