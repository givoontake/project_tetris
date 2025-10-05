#include <iostream>
#include <chrono>
#include <WS2tcpip.h>
#include <fstream>
#include "TestManager.h"

TestManager::TestManager()
{
	for (int i = 0; i < MAX_USER; i++){
		sessions[i] = new Session();
	}
	WSAStartup(MAKEWORD(2, 2), &wsadata);

	//client_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	memset(&server_addr, 0, sizeof(server_addr));
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(PORT_NUM);
	inet_pton(AF_INET, "127.0.0.1", &server_addr.sin_addr);

	iocp_handle = CreateIoCompletionPort(INVALID_HANDLE_VALUE, 0, 0, 0);
}

TestManager::~TestManager()
{
	//closesocket(client_socket);
	WSACleanup();
}

long long TestManager::GetCurrentTimeMS()
{
	return std::chrono::duration_cast<std::chrono::milliseconds>(
		std::chrono::system_clock::now().time_since_epoch()
	).count();
}

void TestManager::AdjustSessionNumber(long long now_time, S2C_TEST_PACKET* p)
{
	static long long delay_time1 = 50;
	static long long delay_time2 = 100;
	static int delay_multiplier = 1;
	static long long last_adjust_time = 0;
	static long long adjust_interval = 20;

	long long adjust_time = adjust_interval * delay_multiplier;
	if (now_time - last_adjust_time < adjust_time) return;
	else last_adjust_time = now_time;

	long long new_delay = now_time - p->last_time;
	if (delay < new_delay) {
		delay += ((new_delay - delay) / 10);
	} 
	else if (delay > new_delay) {
		delay -= ((delay - new_delay) / 10);
	}

	if (delay <= delay_time2) {
		delay_multiplier = 10;
		if (delay <= delay_time1) delay_multiplier = 1;
		if(connected_client.load() < MAX_USER) ConnectToServer();
	}

	if (delay > delay_time2 && connected_client.load() > 20) Disconnect(p->id);

	std::cout << "접속자 수: " << connected_client << " 지연 시간: " << delay << "ms \n";
}

bool TestManager::ConnectToServer()
{
	SOCKET client_socket; // 각 스레드에서 따로 연결을 시도하므로 소켓이 겹치면 안된다.
	client_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	int res = connect(client_socket, (SOCKADDR*)&server_addr, sizeof(server_addr));
	if (res == SOCKET_ERROR) {
		std::cout << "서버 연결 실패: " << WSAGetLastError() << "\n";
		closesocket(client_socket);
		return false;
	}

	int new_index = GetClientIndex();
	if (new_index == -1) return false;
	sessions[new_index]->SetId(new_index); // 이미 배열 인덱스를 id처럼 쓰고 있어서..나중에라도 의미가 있을까?
	sessions[new_index]->SetSocket(client_socket);
	sessions[new_index]->last_send_time = GetCurrentTimeMS();
	sessions[new_index]->remain_data_size = 0;

	//C2S_TEST_LOGIN_PACKET p;
	//p.size = sizeof(C2S_TEST_LOGIN_PACKET);
	//p.type = C2S_TEST_LOGIN;
	//p.temp_id = sessions[new_index]->GetTempId();
	sessions[new_index]->RecvPacket(iocp_handle);

	std::cout << "client[" << new_index << "]" << " connect\n";
	++connected_client;

	CreateIoCompletionPort(reinterpret_cast<HANDLE>(client_socket), iocp_handle, new_index, 0);

	return true;
}

void TestManager::ProcessGQCS()
{
	DWORD transferred_bytes = 0;
	ULONG_PTR key = 0;
	WSAOVERLAPPED* over = nullptr;
	BOOL result = GetQueuedCompletionStatus( // 인자로 넘긴 주소 변수의 값을 채워준다.
		iocp_handle,
		&transferred_bytes,
		&key,
		&over,
		INFINITE);

	ExOverlapped* ex_over = reinterpret_cast<ExOverlapped*>(over);

	if (!result) {
		//std::cout << "client[" << key << "] IOCP 작업 실패! 에러: " << WSAGetLastError() << std::endl;
		Disconnect(static_cast<int>(key));
		if (ex_over->op_type == SEND) delete ex_over;
		return;
	}

	if (transferred_bytes == 0) {
		Disconnect(static_cast<int>(key));
		if (ex_over->op_type == SEND) delete ex_over;
		return;
	}

	switch (ex_over->op_type) {

	case RECV:
		ProcessPacket(transferred_bytes, key);
		sessions[key]->RecvPacket(iocp_handle);
		break;

	case SEND:
		sessions[key]->last_time = GetCurrentTimeMS();
		delete ex_over;
		// 송신 완료 후 추가 처리
		break;
	}
}

void TestManager::ProcessSend()
{
	for (auto& client : sessions) {
		if (client->GetState() != LOBBY) continue;
		long long expected = client->last_send_time;
		long long desired = GetCurrentTimeMS();
		long long ms = desired - expected;
		if (ms > 1000) {
			if (client->last_send_time.compare_exchange_strong(expected, desired)) {
				C2S_TEST_PACKET p;
				int total_packet_size = sizeof(C2S_TEST_PACKET) + sizeof(test_message);
				p.size = static_cast<short>(total_packet_size);
				p.type = C2S_TEST;
				p.id = client->GetId();
				p.last_time = GetCurrentTimeMS();
				char* p_buffer = new char[total_packet_size];
				memcpy(p_buffer, &p, sizeof(C2S_TEST_PACKET)); // 구조체 우선 복사
				memcpy(p_buffer + sizeof(C2S_TEST_PACKET), test_message, sizeof(test_message)); // 구조체 뒤에 붙여서 메세지 복사
				client->SendPacket(p_buffer, iocp_handle);
				std::cout << "TestManager::ProcessSend(): client[" << client->GetIndex() << "] Send Test Packet\n";
				delete[] p_buffer;
			}
		}
		else continue;
	}
}

int TestManager::GetClientIndex()
{
	for (int i = 0; i < MAX_USER; ++i) {
		if (sessions[i]->GetState() == NONE) {
			if (sessions[i]->SetState(NONE, LOGIN)) {
				return i;
			}
		}
	}

	return -1;
}

void TestManager::SetTestMessege(int message_size)
{
	std::ifstream in("message.txt", std::ios::binary);
	if (!in.is_open()) {
		std::cout << "파일 열기 실패, 프로그램을 종료합니다.\n" << std::endl;
		exit(0);
	}
	in.read(test_message, message_size);

	in.close();
}

void TestManager::ProcessPacket(int recv_bytes, int user_index)
{
	if (sessions[user_index]->GetState() == NONE) {
		//std::cout << "handler_interface->GetManagerInterface()->Disconnect(id);\n";
		return;
	}

	if (recv_bytes + sessions[user_index]->GetRemainDataSize() > BUF_SIZE) {
		Disconnect(user_index);
		return;
	}

	else sessions[user_index]->SetRemainDataSize(recv_bytes);

	if (sessions[user_index]->GetRemainDataSize() < sizeof(short)) return;

	short packet_size = sessions[user_index]->GetPacketSize(sessions[user_index]->GetExOver().packet_buf);

	while (sessions[user_index]->GetRemainDataSize() >= packet_size) // 남아있는 데이터 크기가 실제 처리가능한 데이터 크기이상 존재한다면
	{
		packet_size = sessions[user_index]->GetPacketSize(sessions[user_index]->GetExOver().packet_buf);
		char p_buffer[BUF_SIZE];
		// 패킷 분리: packet_buffer에 복사 후 처리
		memcpy(p_buffer, sessions[user_index]->GetExOver().packet_buf, packet_size);
		HandlePacket(p_buffer);

		// 처리한 패킷은 남은 데이터에서 제거
		sessions[user_index]->SetRemainDataSize(-packet_size);
		memmove(sessions[user_index]->GetExOver().packet_buf, sessions[user_index]->GetExOver().packet_buf + packet_size, sessions[user_index]->GetRemainDataSize());
		//handler_interface->HandlePacket(p_buffer);
	}
}

void TestManager::Disconnect(int session_id)
{
	int dis_index = -1;
	for (int i = 0; i < MAX_USER; ++i) {
		if (sessions[i]->GetState() == NONE) continue;
		if (sessions[i]->GetId() == session_id) dis_index = i;
	}
	if (dis_index == -1) return;
	closesocket(sessions[dis_index]->GetSocket());
	sessions[dis_index]->ClearSession();
	sessions[dis_index]->SetState(NONE);
	std::cout << "client[" << dis_index << "]" << " Disconnect\n";
	--connected_client;
}

void TestManager::HandlePacket(char* packet)
{
	S2C_TEST_LOGIN_PACKET* p = reinterpret_cast<S2C_TEST_LOGIN_PACKET*>(packet);
	if (p->id < 0 || p->id >= MAX_USER) return;
	if (sessions[p->id]->GetState() == NONE) return;

	switch (packet[2]) {
	case S2C_TEST_LOGIN: {
		S2C_TEST_LOGIN_PACKET* p = reinterpret_cast<S2C_TEST_LOGIN_PACKET*>(packet);
		for (int i = 0; i < MAX_USER; ++i){ 
			if (sessions[i]->GetState() == LOGIN) { // 다중 클라를 관리해야 하고, 인덱스 != id 상태이므로 그냥 상태를 통해 순서 관계없이 아이디 할당
				sessions[i]->SetState(LOBBY);
				sessions[i]->SetId(p->id);
				std::cout << "client[" << sessions[i]->GetIndex() << "]" << " Login Success\n";
			}
		}
		break;
	}

	case S2C_TEST: {
		long long now_time = GetCurrentTimeMS();
		// 받아서 따로 서버에서 변경되는 패킷 내용이 없다.
		S2C_TEST_PACKET* p = reinterpret_cast<S2C_TEST_PACKET*>(packet);
		AdjustSessionNumber(now_time, p);
		break;
	}
	case S2C_DISCONNECT:
		// 어차피 disconnect는 서버에서 계산해서 보내주니까 필요없지 않나..?
		break;

	default: {
		std::cout << "잘못된 패킷, 코드 수정이 필요합니다. \n";
		break;
	}

	}
}
