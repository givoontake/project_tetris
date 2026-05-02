#pragma once
#include <WinSock2.h>
#include <MSWSock.h>
#include "define.h"

enum OP_TYPE { SEND, RECV, CONNECT };

struct ExOverlapped {
	WSAOVERLAPPED over;
	WSABUF wsabuf;
	char packet_buf[BUF_SIZE];
	OP_TYPE op_type;

	ExOverlapped()
	{
		ZeroMemory(&over, sizeof(over));
		wsabuf.len = BUF_SIZE;
		wsabuf.buf = packet_buf;
	}

	void SetExOverlapped(OP_TYPE type) {
		op_type = type;
	}

	//void SetExOverlapped(OP_TYPE type, char* packet) {
	//	memcpy(socket_buf, packet, packet[0]);
	//	op_type = type;
	//}
};
