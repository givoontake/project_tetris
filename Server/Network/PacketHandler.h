#pragma once
#include "types.h"

class IOCPServer;
class Session;

class PacketHandler
{
	IOCPServer& server_;

public:
	PacketHandler(IOCPServer& server);
	void HandlePacket(char* packet, const SP<Session>& session);

private:
	void HandleLoginPacket(char* packet, const SP<Session>& session);
	void HandleMessagePacket(char* packet, const SP<Session>& session);
	void HandleTestPacket(char* packet, const SP<Session>& session);
	void HandleDisconnectPacket(const SP<Session>& session);
	void HandleJoinPublicRoomPacket(char* packet, const SP<Session>& session);
	void HandleJoinPrivateRoomPacket(char* packet, const SP<Session>& session);
	void HandleFastMatchingPacket(char* packet, const SP<Session>& session);
	void HandleAddFriendRequestPacket(char* packet, const SP<Session>& session);
	void HandleAcceptFriendPacket(char* packet, const SP<Session>& session);
	void HandleDeleteFriendPacket(char* packet, const SP<Session>& session);
};

