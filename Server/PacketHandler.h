#pragma once
#include "Types.h"

class IOCPServer;
class Session;

class PacketHandler
{
	IOCPServer& server;

public:
	PacketHandler(IOCPServer& server);
	void HandlePacket(char* packet, Session& session);

private:
	void HandleLoginPacket(char* packet, const SP<Session>& session);
	void HandleMessagePacket(char* packet, Session& session);
	void HandleTestPacket(char* packet, Session& session);
	void HandleDisconnectPacket(Session& session);
	void HandleJoinOpenRoomPacket(char* packet, Session& session);
	void HandleJoinLockRoomPacket(char* packet, Session& session);
	void HandleFastMatchingPacket(char* packet, Session& session);
	void HandleRequestFriendPacket(char* packet, Session& session);
	void HandleAcceptFriendPacket(char* packet, Session& session);
	void HandleDeleteFriendPacket(char* packet, Session& session);
};

