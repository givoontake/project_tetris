#pragma once
class IOCPServer;
class Session;

class PacketHandler
{
	IOCPServer& server_;

public:
	PacketHandler(IOCPServer& server);
	void HandlePacket(char* packet, Session* session);

private:
	void HandleLoginPacket(char* packet, Session* session);
	void HandleMessagePacket(char* packet, Session* session);
	void HandleTestPacket(char* packet, Session* session);
	void HandleDisconnectPacket(Session* session);
	void HandleJoinPublicRoomPacket(char* packet, Session* session);
	void HandleJoinPrivateRoomPacket(char* packet, Session* session);
	void HandleFastMatchingPacket(char* packet, Session* session);
	void HandleAddFriendRequestPacket(char* packet, Session* session);
	void HandleAcceptFriendPacket(char* packet, Session* session);
	void HandleDeleteFriendPacket(char* packet, Session* session);
};

