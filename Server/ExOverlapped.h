#pragma once
#include <WinSock2.h>
#include <MSWSock.h>
#include "define.h"

enum OP_TYPE { SEND, RECV, ACCEPT};

struct ExOverlapped {
	WSAOVERLAPPED over;
	WSABUF wsabuf;
	char packet_buf[BUF_SIZE];
	OP_TYPE op_type;
	int operation_id; // 세션의 id와 같음

	ExOverlapped()
	{
		ZeroMemory(&over, sizeof(over));
		wsabuf.len = BUF_SIZE;
		wsabuf.buf = packet_buf;
		operation_id = -1;
	}

	void SetOperationType(OP_TYPE type) {
		op_type = type;
	}

	void SetOperationId(int id) {
		operation_id = id;
	}

	//void SetExOverlapped(OP_TYPE type, char* packet) {
	//	memcpy(socket_buf, packet, packet[0]);
	//	op_type = type;
	//}
};