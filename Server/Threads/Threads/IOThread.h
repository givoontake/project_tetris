#pragma once
#include <WinSock2.h>
#include "ServerThread.h"
#include "types.h"

class IOCPServer;
class Session;
struct ExOverlapped;
struct SendBuffer;

class IOThread final : public ServerThread
{
	IOCPServer& iocp_server_;

	void ProcessAcceptCompletion(BOOL result, ExOverlapped* ex_over);
	void ProcessSessionCompletion(BOOL result, DWORD transferred_bytes, ExOverlapped* ex_over);
	void ProcessReceiveCompletion(BOOL result, DWORD transferred_bytes, const SP<Session>& session);
	void ProcessSendCompletion(BOOL result, DWORD transferred_bytes, SendBuffer* send_buffer, const SP<Session>& session);
	void ProcessRoomCompletion(ExOverlapped* ex_over);
	void ProcessSessionDBCompletion(BOOL result, ExOverlapped* ex_over);
	void ProcessServerDBCompletion(BOOL result, ExOverlapped* ex_over);

public:
	IOThread(IOCPServer& iocp_server);
	void Run() override;
	void Close() override;
};
