#pragma once
#include <WinSock2.h>
#include <MSWSock.h>
#include "define.h"
#include "enum_class.h"
#include "DBResult.h"

struct ExOverlapped {
	WSAOVERLAPPED over;
	int operation_id; // 세션 id와 같다.
	OP_TYPE op_type;

	ExOverlapped() {
		ZeroMemory(&over, sizeof(over));
		operation_id = -1;
	}
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

	void SetOperationId(int id) {
		ex_over.operation_id = id;
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

