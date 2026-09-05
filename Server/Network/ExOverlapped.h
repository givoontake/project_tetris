#pragma once
#include <cstdint>
#include <memory>
#include <WinSock2.h>
#include <MSWSock.h>
#include "settings.h"
#include "enum_class.h"
#include "DBResult.h"

struct SessionKey {
	int session_index = -1;
	int player_id = -1;
	std::uint64_t session_id = 0;
};

constexpr ULONG_PTR LISTEN_IO_COMPLETION = 1;
constexpr ULONG_PTR SESSION_IO_COMPLETION = 2;
constexpr ULONG_PTR DB_SESSION_COMPLETION = 4;
constexpr ULONG_PTR DB_SERVER_COMPLETION = 5;

struct ExOverlapped {
	WSAOVERLAPPED over;
	OPType op_type;
	SessionKey session_key;

	ExOverlapped() {
		ZeroMemory(&over, sizeof(over));
	}
};

struct IOOverlapped {
	ExOverlapped ex_over;
	WSABUF wsabuf;
	char packet_buffer[BUF_SIZE];

	IOOverlapped()
	{
		wsabuf.len = BUF_SIZE;
		wsabuf.buf = packet_buffer;
	}

	void SetOperationType(OPType op_type) {
		ex_over.op_type = op_type;
	}

};

struct DBOverlapped
{
	ExOverlapped ex_over;
	std::unique_ptr<DBResultBase> result_data;

	explicit DBOverlapped(DBOperationType operation_type) : result_data(std::make_unique<DBResultFailure>(operation_type)) {}
};

