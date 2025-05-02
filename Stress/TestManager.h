#pragma once
#include <WinSock2.h>
#include <MSWSock.h>
#include <array>
#include <atomic>
#include "Exoverlapped.h"
#include "Session.h"
#include "PacketHandler.h"

#pragma comment(lib, "WS2_32.lib")
#pragma comment(lib, "MSWSock.lib")

class TestManager : ITestManager
{
	HANDLE iocp_handle;
	//SOCKET client_socket;
	WSADATA wsadata;
	SOCKADDR_IN server_addr;
	std::array<Session, MAX_USER> clients;
	PacketHandler packet_handler;

public: // 테스트하는데 굳이 private 할 이유는 없다
	std::atomic<int> connected_client = 0;
	char test_message[BUF_SIZE];

public:
	TestManager();
	~TestManager();

	virtual std::array<Session, MAX_USER>& GetSessionList() override;
	virtual int GetCurrentTimeMS() override;
	virtual void AdjustClientNumber(int now_time, S2C_TEST_PACKET* p) override;

	bool ConnectToServer();
	void ProcessGQCS();
	void ProcessSend();
	int GetClientId();
	void SetTestMessege(int message_size);
	void Disconnect(int user_id);
};

