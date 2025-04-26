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

class IOCPServer : IServer
{
	HANDLE iocp_handle;
	SOCKET listen_socket, client_socket;
	WSADATA wsadata;
	SOCKADDR_IN server_addr;
	ExOvelapped accept_over;

	std::array<Session, MAX_USER> users;

	PacketHandler packet_handler;
	bool is_running = true;
	
public:
	IOCPServer();
	~IOCPServer();

	virtual std::array<Session, MAX_USER>& GetSessionList() override;

	void StartServer();
	void ProcessGQCS();
	int GetUserId();
	void Disconnect(int user_id);
};

