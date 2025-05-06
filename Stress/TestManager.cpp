#include <iostream>
#include <chrono>
#include <WS2tcpip.h>
#include <fstream>
#include "TestManager.h"

TestManager::TestManager()
{
	packet_handler = std::make_unique<PacketHandler>(this);
	for (auto& client : clients) {
		client = std::make_unique<Session>(packet_handler.get()); // packet_handler는 unique_ptr이므로 get()을 이용해 raw ptr을 넘긴다.
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

std::array<std::unique_ptr<Session>, MAX_USER>& TestManager::GetSessionList()
{
	return clients;
}

long long TestManager::GetCurrentTimeMS()
{
	return std::chrono::duration_cast<std::chrono::milliseconds>(
		std::chrono::system_clock::now().time_since_epoch()
	).count();
}

void TestManager::AdjustClientNumber(long long now_time, S2C_TEST_PACKET* p)
{
	long long ms = now_time - p->last_time;
	if (ms > 100) { // 딜레이가 100ms 이상이면
		Disconnect(p->id);
	}
	else {
		ConnectToServer();
	}

	std::cout << "\r접속자 수: " << connected_client << " 지연 시간: " << ms << "ms     " << std::flush;
}

bool TestManager::ConnectToServer()
{
	SOCKET client_socket; // 각 스레드에서 따로 연결을 시도하므로 소켓이 겹치면 안된다.
	client_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	int res = connect(client_socket, (SOCKADDR*)&server_addr, sizeof(server_addr));
	if (res == SOCKET_ERROR) {
		std::cout << "서버 연결 실패: " << WSAGetLastError() << "\n";
		closesocket(client_socket);
		WSACleanup();
		return false;
	}

	int new_id = GetClientId();
	clients[new_id]->SetId(new_id); // 이미 배열 인덱스를 id처럼 쓰고 있어서..나중에라도 의미가 있을까?
	clients[new_id]->SetSocket(client_socket);
	clients[new_id]->last_send_time = GetCurrentTimeMS();
	
	while (true) {
		int expected = connected_client;
		int desired = expected + 1;
		if (connected_client.compare_exchange_strong(expected, desired)) break;
	}
	CreateIoCompletionPort(reinterpret_cast<HANDLE>(client_socket), iocp_handle, new_id, 0);

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

	ExOvelapped* ex_over = reinterpret_cast<ExOvelapped*>(over);

	if (!result) {
		// 서버가 종료되었을 경우?
	}

	switch (ex_over->op_type) {

	case RECV: {
		clients[key]->MergePacket(transferred_bytes, ex_over->packet_buf);
		clients[key]->RecvPacket();
		break;
	}

	case SEND:
		clients[key]->last_time = GetCurrentTimeMS();
		C2S_TEST_PACKET* p = reinterpret_cast<C2S_TEST_PACKET*>(ex_over->packet_buf);
		std::cout << "=== C2S_TEST_PACKET ===" << std::endl;
		std::cout << "Size: " << p->size << std::endl;
		std::cout << "Type: " << p->type << std::endl;
		std::cout << "ID: " << p->id << std::endl;
		std::cout << "Message: " << p->message << std::endl;
		std::cout << "Last Time: " << p->last_time << std::endl;
		std::cout << "=======================" << std::endl;
		std::cout << std::endl;
		delete ex_over;
		// 송신 완료 후 추가 처리
		break;
	}
}

void TestManager::ProcessSend()
{
	for (auto& client : clients) {
		if (!client->GetUse()) continue;
		long long expected = client->last_send_time;
		long long desired = GetCurrentTimeMS();
		long long ms = desired - expected;
		if (ms > 1000) {
			if (client->last_send_time.compare_exchange_strong(expected, desired)) {
				C2S_TEST_PACKET p;
				p.size = sizeof(C2S_TEST_PACKET);
				p.type = C2S_TEST;
				p.id = client->GetId();
				memcpy(p.message, test_message, sizeof(test_message));
				p.last_time = GetCurrentTimeMS();
				client->SendPacket(reinterpret_cast<char*>(&p));
			}
		}
		else continue;
	}
}

int TestManager::GetClientId()
{
	for (int i = 0; i < MAX_USER; ++i) {
		bool expected = false;
		if (clients[i]->SetUse(false, true)) {
			return i;
		}
	}

	return -1;
}

void TestManager::SetTestMessege(int message_size)
{
	std::ifstream in("message.txt", std::ios::binary);
	in.read(test_message, message_size);
	if (!in.is_open()) {
		std::cout << "파일 열기 실패, 프로그램을 종료합니다.\n" << std::endl;
		exit(0);
	}

	std::cout << test_message << std::endl;
	in.close();
}

void TestManager::Disconnect(int user_id)
{
	// 채팅에 연결된 모든 클라에게 disconnect 패킷 전송-> 실시간 채팅도 아니고 필요 없을 듯 한데..
	clients[user_id]->SetUse(true, false); // 원래는 카스는 필요 없긴 한데.. 함수를 또 만드는게 번거로워서 그냥 하나에 만들었다.
	closesocket(clients[user_id]->GetSocket());
	while (true) {
		int expected = connected_client;
		int desired = expected - 1;
		if (connected_client.compare_exchange_strong(expected, desired)) break;
	}
}
