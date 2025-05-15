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

class IOCPServer : public IServer
{
	HANDLE iocp_handle;
	SOCKET listen_socket, client_socket;
	WSADATA wsadata;
	SOCKADDR_IN server_addr;
	ExOvelapped accept_over;
	MQueue disconnect_queue;

	std::unique_ptr<PacketHandler> packet_handler; // 먼저 선언되어 있다면 해당 변수는 나중에 선언되는 변수에서 사용 가능하다.
	std::array<std::unique_ptr<Session>, MAX_USER> users;
	// 변수-> 컨테이너 생성 시 객체 생성자에 인자 넣는게 안된다.
	// 포인터 -> 생성자에서 인자 넣고 동적할당 하면 된다.

	bool is_running = true;
	
public:
	IOCPServer();
	~IOCPServer();

	virtual std::array<std::unique_ptr<Session>, MAX_USER>& GetSessionList() override;
	virtual MQueue& GetQueue() override;
	virtual void Disconnect(int user_id) override;

	void StartServer();
	void ProcessGQCS();
	int GetUserId();
	
};

