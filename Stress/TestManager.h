#pragma once
#include <WinSock2.h>
#include <MSWSock.h>
#include <array>
#include <atomic>
#include <memory>
#include "ExOverlapped.h"
#include "Session.h"
#include "MQueue.h"

#pragma comment(lib, "WS2_32.lib")
#pragma comment(lib, "MSWSock.lib")

class TestManager
{
	HANDLE iocp_handle;
	//SOCKET client_socket;
	WSADATA wsadata;
	SOCKADDR_IN server_addr;
	std::array<Session*, MAX_USER> sessions;

public: // 테스트하는데 굳이 private 할 이유는 없다
	std::atomic<int> connected_client = 0;
	char test_message[MAX_MESSAGE_SIZE];
	long long delay = 0;

public:
	TestManager();
	~TestManager();

	bool ConnectToServer();
	void ProcessGQCS();
	void ProcessSend();
	int GetClientIndex();
	void SetTestMessege(int message_size);
	void ProcessPacket(int recv_bytes, int user_index);
	void HandlePacket(char* packet);
	void Disconnect(int session_index);

	long long GetCurrentTimeMS();
	void AdjustSessionNumber(long long now_time, S2C_TEST_PACKET* p);
};

