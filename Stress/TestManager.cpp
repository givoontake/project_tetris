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

MQueue& TestManager::GetQueue()
{
	return disconnect_queue;
}

long long TestManager::GetCurrentTimeMS()
{
	return std::chrono::duration_cast<std::chrono::milliseconds>(
		std::chrono::system_clock::now().time_since_epoch()
	).count();
}

void TestManager::AdjustClientNumber(long long now_time, S2C_TEST_PACKET* p)
{
	long long new_delay = now_time - p->last_time;
	if (delay < new_delay) {
		++delay;
	}
	else if (delay > new_delay) {
		--delay;
	}

	if (delay > 100) { // 딜레이가 100ms 이상이면
		Disconnect(p->id);
	}
	else {
		ConnectToServer();
	}

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
		WSACleanup();
		return false;
	}

	int new_id = GetClientId();
	clients[new_id]->SetId(new_id); // 이미 배열 인덱스를 id처럼 쓰고 있어서..나중에라도 의미가 있을까?
	clients[new_id]->SetSocket(client_socket);
	clients[new_id]->last_send_time = GetCurrentTimeMS();
	clients[new_id]->remain_data_size = 0;
	clients[new_id]->RecvPacket();

	std::cout << "client[" << new_id << "]" << " connect\n";
	
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
		clients[key]->ProcessPacket(transferred_bytes, key, result);
		clients[key]->RecvPacket();
		break;

	case SEND:
		clients[key]->last_time = GetCurrentTimeMS();
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
				p.last_time = GetCurrentTimeMS();
				p.message_size = sizeof(test_message);
				int real_size = sizeof(C2S_TEST_PACKET) + sizeof(test_message);
				char* pp = new char[real_size];
				memcpy(pp, &p, sizeof(C2S_TEST_PACKET));
				memcpy(pp + sizeof(C2S_TEST_PACKET), test_message, sizeof(test_message));
				client->SendPacket(pp);
				delete[] pp;
			}
		}
		else continue;
	}
}

int TestManager::GetClientId()
{
	for (int i = 0; i < MAX_USER; ++i) {
		if (!clients[i]->GetUse()) {
			if (clients[i]->SetUse(false, true)) {
				return i;
			}
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

	in.close();
}

void TestManager::Disconnect(int client_id)
{
	std::cout << "client[" << client_id << "]" << " Disconnect\n";
	clients[client_id]->SetUse(true, false); // 원래는 카스는 필요 없긴 한데.. 함수를 또 만드는게 번거로워서 그냥 하나에 만들었다.
	closesocket(clients[client_id]->GetSocket());
	while (true) {
		int expected = connected_client;
		int desired = expected - 1;
		if (connected_client.compare_exchange_strong(expected, desired)) break;
	}
}
