#pragma once
class IOCPServer;
class Session;

class PacketHandler
{
	IOCPServer& server;

public:
	PacketHandler(IOCPServer& server);
	void HandlePacket(char* packet, Session& session, int request_gen);

private:
	void HandleLoginPacket(char* packet, Session& session, int request_gen);
	void HandleMessagePacket(char* packet, Session& session, int request_gen);
	void HandleTestPacket(char* packet, Session& session);
	void HandleDisconnectPacket(Session& session);
	void HandleJoinOpenRoomPacket(char* packet, Session& session, int request_gen);
	void HandleJoinLockRoomPacket(char* packet, Session& session, int request_gen);
	void HandleFastMatchingPacket(char* packet, Session& session, int request_gen);
	void HandleRequestFriendPacket(char* packet, Session& session, int request_gen);
	void HandleAcceptFriendPacket(char* packet, Session& session, int request_gen);
	void HandleDeleteFriendPacket(char* packet, Session& session, int request_gen);
};

