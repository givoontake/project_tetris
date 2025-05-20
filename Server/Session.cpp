#include <iostream>
#include "Session.h"

Session::Session(IPacketHandler* p_handler) :  handler_interface(p_handler)
{
	recv_over.SetExOverlapped(RECV);
}

void Session::SendPacket(char* packet)
{
	if (!in_use) return;
	ExOvelapped* send_over = new ExOvelapped;
	send_over->SetExOverlapped(SEND);
	short packet_size = GetPacketSize(packet);
	memcpy(send_over->packet_buf, packet, packet_size);
	send_over->wsabuf.len = packet_size;
	int ret = WSASend(socket, &send_over->wsabuf, 1, 0, 0, &send_over->over, 0);
	if (ret == SOCKET_ERROR && WSAGetLastError() != WSA_IO_PENDING) {
		// WSA함수로 IOCP등록 시 발생하는 오류는 GQCS로 가지 않음 → 직접 처리해야 함
		//std::cout << "\r client[" << id << "]" << "WSASend() IOCP Sign Up Fail";
		handler_interface->GetServerInterface()->GetQueue().EnQ(id);
		delete send_over;
	}
}

void Session::RecvPacket()
{
	if (!in_use) return;
	DWORD recv_flag = 0;
	ZeroMemory(&recv_over.over, sizeof(recv_over.over)); // iocp 작업을 할 때마다 오버랩 구조체 초기화 필요(안정성)
	recv_over.wsabuf.len = BUF_SIZE - remain_data_size;
	recv_over.wsabuf.buf = recv_over.packet_buf + remain_data_size;
	int ret = WSARecv(socket, &recv_over.wsabuf, 1, 0, &recv_flag,
		&recv_over.over, 0);
	if (ret == SOCKET_ERROR && WSAGetLastError() != WSA_IO_PENDING) {
		//std::cout << "\r client[" << id << "]" << "WSARecv() IOCP Sign Up Fail";
		handler_interface->GetServerInterface()->GetQueue().EnQ(id);
	}
}

void Session::MergePacket(int recv_bytes, char* recv_data) // 세션에 있는게 맞는 것 같다. 나중에 방이 추가되면, 방에서도 세션에 접근만 해서 보내기만 하면 된다.
{
    // 일단 새로 들어온 데이터를 뒤에 붙임
	memcpy(recv_over.packet_buf + remain_data_size, recv_data, recv_bytes);
	remain_data_size += recv_bytes;

	short packet_size = GetPacketSize(recv_over.packet_buf);

	if (remain_data_size + packet_size > BUF_SIZE) handler_interface->GetServerInterface()->GetQueue().EnQ(id);

	while (remain_data_size >= packet_size) // 남아있는 데이터 크기가 실제 처리가능한 데이터 크기이상 존재한다면
	{
		char p_buffer[BUF_SIZE];
		// 패킷 분리: packet_buffer에 복사 후 처리
		memcpy(p_buffer, recv_over.packet_buf, packet_size);
		// ProcessRecvPacket(p_buffer);

		 // 처리한 패킷은 남은 데이터에서 제거
		remain_data_size -= packet_size;
		memmove(recv_over.packet_buf, recv_over.packet_buf + packet_size, remain_data_size);
		handler_interface->ProcessPacket(p_buffer);
	}
}

short Session::GetPacketSize(char* packet)
{
	switch (packet[2]) {
	case S2C_TEST: {
		S2C_TEST_PACKET* p = reinterpret_cast<S2C_TEST_PACKET*>(packet);
		short packet_size;
		memcpy(&packet_size, packet, sizeof(packet_size));
		packet_size += p->message_size;
		return packet_size;
	}
	case C2S_TEST: {
		C2S_TEST_PACKET* p = reinterpret_cast<C2S_TEST_PACKET*>(packet);
		short packet_size;
		memcpy(&packet_size, packet, sizeof(packet_size));
		packet_size += p->message_size;
		return packet_size;
	}
	default: {
		short packet_size;
		memcpy(&packet_size, packet, sizeof(packet_size));
		return packet_size;
	}
	}
}

bool Session::SetUse(bool expected, bool desired)
{
	// false -> true
	if (desired == true) {
		if (in_use.compare_exchange_strong(expected, desired)) return true;
		else return false;
	}

	// true -> false
	else {
		if (in_use.compare_exchange_strong(expected, desired)) return true;
		else return false;
	}
}
