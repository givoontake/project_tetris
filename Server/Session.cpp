#include <iostream>
#include "Session.h"
#include "packetType.h"

Session::Session()
{
	recv_over.SetExOverlapped(RECV);
}

void Session::InitSession(int new_id, SOCKET new_socket)
{
	id = new_id;
	socket = new_socket;
	remain_data_size = 0;
	room_id = -1;
	disconnect_flag = false;
	state = LOBBY;
}

void Session::SendPacket(char* packet)
{
	if (state == NONE) return;
	ExOverlapped* send_over = new ExOverlapped;
	send_over->SetExOverlapped(SEND);
	short packet_size = GetPacketSize(packet);
	memcpy(send_over->packet_buf, packet, packet_size);
	send_over->wsabuf.len = packet_size;
	int ret = WSASend(socket, &send_over->wsabuf, 1, 0, 0, &send_over->over, 0);
	if (ret == SOCKET_ERROR && WSAGetLastError() != WSA_IO_PENDING) {
		//state = false; // Disconnect 패킷은 나중에 보내더라도, 서버에서 이 세션에 대해 더이상 처리할 필요는 없다.
		// WSA함수로 IOCP등록 시 발생하는 오류는 GQCS로 가지 않음 → 직접 처리해야 함
		//std::cout << "\r client[" << id << "]" << "WSASend() IOCP Sign Up Fail";
		//handler_interface->GetServerInterface()->GetQueue().EnQ(id);
		disconnect_flag = true;
		delete send_over;
		return;
	}
	++remainning_send_IOCP;
	++remainning_total_IOCP;
}

void Session::RecvPacket()
{
	if (state == NONE) return;
	DWORD recv_flag = 0;
	ZeroMemory(&recv_over.over, sizeof(recv_over.over)); // iocp 작업을 할 때마다 오버랩 구조체 초기화 필요(안정성)
	recv_over.wsabuf.len = BUF_SIZE - remain_data_size;
	recv_over.wsabuf.buf = recv_over.packet_buf + remain_data_size;
	int ret = WSARecv(socket, &recv_over.wsabuf, 1, 0, &recv_flag,
		&recv_over.over, 0);
	if (ret == SOCKET_ERROR && WSAGetLastError() != WSA_IO_PENDING) {
		//std::cout << "\r client[" << id << "]" << "WSARecv() IOCP Sign Up Fail";
		//handler_interface->GetServerInterface()->GetQueue().EnQ(id);
		disconnect_flag = true;
		return;
	}
	++remainning_total_IOCP;
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

bool Session::SetUse(USER_STATE expected, USER_STATE desired)
{
	// false -> true
	if (desired != expected) {
		if (state.Compare_exchange_strong(expected, desired)) return true;
		else return false;
	}

	// 같으면 원하는 상태이므로 true 반환
	return true;
}
