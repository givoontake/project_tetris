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

//void Session::ProcessPacket(int recv_bytes)
//{
//	if (in_use == false) {
//		//std::cout << "handler_interface->GetManagerInterface()->Disconnect(id);\n";
//		return;
//	}
//
//	// 모든 패킷 처리는 중앙 서버에서 하도록 변경할 예정
//	// 중앙 서버의 디스커넥트에 직접 접근하도록 하지 말고, 디스커넥트 요청이 온 것처럼(예전에 생각한 방식)처리.. 아니다. 굳이 번거롭게 이러지 말자
//	// 그냥 disconnect 처리 여부를 담당하는 플래그를 하나 만들자. 서버에서 패킷을 처리할 때 마다 이 플래그를 검사하도록 하자.
//	// 그러면 간편해진다.
//	if (recv_bytes + remain_data_size > BUF_SIZE) {
//		//handler_interface->GetServerInterface()->Disconnect(id);
//		disconnect_flag = true;
//		return;
//	}
//
//	else remain_data_size += recv_bytes;
//
//	if (remain_data_size < sizeof(short)) return;
//
//	short packet_size = GetPacketSize(recv_over.packet_buf);
//
//	// ---구조를 변경하면서 고민되는 점---
//	// 단순히 세션이 처리 가능한 데이터가 있다고 작업 큐에 넣느냐-> 나중에 서버단에서 처리 후 데이터 땡기기 등 처리를 해야함
//	// 세션에 작업 가능한 데이터를 잘라 따로 보관해 놓는다-> 나중에 서버가 따로 신경 쓸 부분은 없어짐.. 그러나 추가 메모리 필요
//	// 뭐가 나으려나??
//	while (remain_data_size >= packet_size) // 남아있는 데이터 크기가 실제 처리가능한 데이터 크기이상 존재한다면
//	{
//		packet_size = GetPacketSize(recv_over.packet_buf);
//		char p_buffer[BUF_SIZE];
//		// 패킷 분리: packet_buffer에 복사 후 처리
//		memcpy(p_buffer, recv_over.packet_buf, packet_size);
//		// ProcessRecvPacket(p_buffer);
//
//		 // 처리한 패킷은 남은 데이터에서 제거
//		remain_data_size -= packet_size;
//		memmove(recv_over.packet_buf, recv_over.packet_buf + packet_size, remain_data_size);
//		//handler_interface->HandlePacket(p_buffer);
//	}
//	// 
//}

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
