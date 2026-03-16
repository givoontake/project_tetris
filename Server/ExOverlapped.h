#pragma once
#include <WinSock2.h>
#include <MSWSock.h>
#include "define.h"
#include "enum_class.h"
#include "DBResult.h"

struct ExOverlapped {
	WSAOVERLAPPED over;
	OP_TYPE op_type;
	int request_id = -1;

	ExOverlapped() {
		ZeroMemory(&over, sizeof(over));
	}

	void SetRequestId(int new_id) { request_id = new_id; }
};

struct IOOverlapped {
	ExOverlapped ex_over;
	WSABUF wsabuf;
	char packet_buf[BUF_SIZE];

	IOOverlapped()
	{
		wsabuf.len = BUF_SIZE;
		wsabuf.buf = packet_buf;
	}

	void SetOperationType(OP_TYPE type) {
		ex_over.op_type = type;
	}

	//void SetExOverlapped(OP_TYPE type, char* packet) {
	//	memcpy(socket_buf, packet, packet[0]);
	//	op_type = type;
	//}
};

struct DBOverlapped
{
	ExOverlapped ex_over;
	DBOperationType type{};
	bool ok;
	std::unique_ptr<DBResultDefault> result_data;

	DBOverlapped() {
		ok = false;
		result_data = nullptr;
	}
};

