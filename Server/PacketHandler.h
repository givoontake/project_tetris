#pragma once
#include "Interface.h"

class PacketHandler : public IPacketHandler
{
	IServer* server_interface;
public:
	PacketHandler(IServer* i_server);
	virtual void HandlePacket(char* pakcet) override;
	virtual void Disconnect(int user_id) override; 
	virtual IServer* GetServerInterface() const override { return server_interface; }
};
