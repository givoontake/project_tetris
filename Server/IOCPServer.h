#pragma once
#include <WinSock2.h>
#include <MSWSock.h>
#include <array>
#include <atomic>
#include "Exoverlapped.h"
#include "Session.h"

#pragma comment(lib, "WS2_32.lib")
#pragma comment(lib, "MSWSock.lib")

class IOCPServer
{
	HANDLE iocp_handle;
	SOCKET listen_socket, client_socket;
	WSADATA wsadata;
	SOCKADDR_IN server_addr;
	ExOvelapped accept_over;

	std::array<std::atomic<bool>, MAX_USER> id_container;
	std::array<Session, MAX_USER> users;

	bool is_running = true;
	
public:
	IOCPServer();

	void StartServer();
	void ProcessGQCS();
	int GetUserId();
	void Disconnect(int user_id);
};

