#pragma once
#include <WinSock2.h>
#include <MSWSock.h>
#include "define_packets.h"
#include "enum_class.h"
#include "DBResult.h"

struct SessionKey {
	int index = -1;
	int id = -1;
};

constexpr ULONG_PTR LISTEN_IO_COMPLETION = 1;
constexpr ULONG_PTR SESSION_IO_COMPLETION = 2;
constexpr ULONG_PTR ROOM_IO_COMPLETION = 3;
constexpr ULONG_PTR DB_IO_COMPLETION = 4;
constexpr ULONG_PTR DB_INIT_SERVER_COMPLETION = 5;

struct ExOverlapped {
	WSAOVERLAPPED over;
	OP_TYPE op_type;
	SessionKey key;
	int room_index = -1;

	ExOverlapped() {
		ZeroMemory(&over, sizeof(over));
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

