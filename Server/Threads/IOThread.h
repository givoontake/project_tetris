#pragma once
#include "ServerThread.h"

class IOCPServer;

class IOThread final : public ServerThread
{
	IOCPServer& iocp_server_;

public:
	IOThread(IOCPServer& iocp_server);
	void Run() override;
	void Close() override;
};
