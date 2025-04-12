#pragma once
#include <WinSock2.h>
#include <MSWSock.h>
#include "define.h"

#pragma comment(lib, "WS2_32.lib")
#pragma comment(lib, "MSWSock.lib")

enum OP_TYPE {SEND, RECV, ACCEPT};

struct ExOvelapped {
	WSAOVERLAPPED over;
	WSABUF wsabuf;
	char socket_buf[BUF_SIZE];
	OP_TYPE op_type;

	ExOvelapped()
	{
		ZeroMemory(&over, sizeof(over));
		wsabuf.len = BUF_SIZE;
		wsabuf.buf = socket_buf;
	}

	void SetExOverlapped(OP_TYPE type) {
		op_type = type;
	}

	void SetExOverlapped(OP_TYPE type, char* packet) {
		memcpy(socket_buf, packet, packet[0]);
		op_type = type;
	}
};

class IOCPServer
{
	HANDLE iocp_handle;
	SOCKET listen_socket, client_socket;
	WSADATA wsadata;
	SOCKADDR_IN server_addr;
	ExOvelapped accept_over;
	
public:
	IOCPServer();
	void StartServer();
};

