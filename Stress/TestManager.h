#pragma once
#include <WinSock2.h>
#include <MSWSock.h>
#include <array>
#include <atomic>
#include <memory>
#include "Exoverlapped.h"
#include "Session.h"
#include "PacketHandler.h"
#include "MQueue.h"

#pragma comment(lib, "WS2_32.lib")
#pragma comment(lib, "MSWSock.lib")

class TestManager : public ITestManager
{
	HANDLE iocp_handle;
	//SOCKET client_socket;
	WSADATA wsadata;
	SOCKADDR_IN server_addr;
	std::array<std::unique_ptr<Session>, MAX_USER> clients;
	std::unique_ptr<PacketHandler> packet_handler;
	MQueue disconnect_queue;

public: // 테스트하는데 굳이 private 할 이유는 없다
	std::atomic<int> connected_client = 0;
	char test_message[MAX_MESSAGE_SIZE];
	long long delay = 0;

public:
	TestManager();
	~TestManager();

	virtual std::array<std::unique_ptr<Session>, MAX_USER>& GetSessionList() override;
	virtual long long GetCurrentTimeMS() override;
	virtual MQueue& GetQueue() override;
	virtual void AdjustClientNumber(long long now_time, S2C_TEST_PACKET* p) override;
	virtual void Disconnect(int client_id) override;

	bool ConnectToServer();
	void ProcessGQCS();
	void ProcessSend();
	int GetClientId();
	void SetTestMessege(int message_size);
};

