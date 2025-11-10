#pragma once

class IOCPServer;

class PacketHandler
{
protected:
	IOCPServer* server;
public:
	PacketHandler(IOCPServer* server);
	virtual void HandlePacket(char* pakcet, int user_index);
};

