#pragma once
#include <windows.h>
#include <string>

#pragma comment(lib, "ws2_32.lib")

class Session
{
private:
	SOCKET ClientSocket;
	const char* ServerIP = "127.0.0.1";
	int ServerPort = 1234;
	int id = -1;

public:
	Session();
	~Session();

	bool ConnectToServer();
	void Disconnect();
	void ProcessRecvPacket(char* packet);
	void MergePacket(int recv_bytes, char* recv_data);
	void ProcessSendPacket(std::string message);
	void MergePacket(char* packet);

	SOCKET GetSocket() const { return ClientSocket; }
};

