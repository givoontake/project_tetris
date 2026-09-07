#pragma once
#include <WinSock2.h>
#include <MSWSock.h>
#include "define.h"

enum class OperationType
{
	SEND,
	RECV,
	CONNECT
};

struct ExOverlapped
{
	WSAOVERLAPPED over{};
	WSABUF wsabuf{};
	char packet_buffer[BUF_SIZE]{};
	OperationType operation_type = OperationType::RECV;

	ExOverlapped()
	{
		wsabuf.len = BUF_SIZE;
		wsabuf.buf = packet_buffer;
	}

	void SetOperationType(OperationType new_operation_type)
	{
		operation_type = new_operation_type;
	}
};
