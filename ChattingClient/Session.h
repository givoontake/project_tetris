#pragma once
#include <windows.h>
#include <string>

#pragma comment(lib, "ws2_32.lib")

class Session
{
private:
	SOCKET clientSocket;
	SOCKET flaskSocket;
	const char* serverIP = "127.0.0.1";
	const char* flaskIP = "127.0.0.1";
	int flaskPort = 5000;
	int serverPort = 1234;
	int id = -1;

public:
	Session();
	~Session();

	bool ConnectToServer();
	bool ConnectToLocalFlask();
	void Disconnect();
	void ProcessRecvPacket(char* packet);
	void MergePacket(int recv_bytes, char* recv_data);
	void ProcessSendPacket(std::string message, int send_type);

	SOCKET GetSocket() const { return clientSocket; }
};

