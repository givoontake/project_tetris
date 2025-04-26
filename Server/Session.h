#pragma once
#include <WinSock2.h>
#include <MSWSock.h>
#include <mutex>
#include <atomic>
#include "ExOverlapped.h"
#include "Interface.h"

class Session
{
	SOCKET socket;
	ExOvelapped recv_over;
	std::mutex session_mutex;
	IPacketHandler* handler_interface;
	int id = -1;

	// 남은 데이터는 recv_over 버퍼에 들어 있으므로 추가로 만들 필요가 없음.
	int remain_data_size = 0;

	std::atomic<bool> in_use = false;
	
public:
	Session();

	void SendPacket(char* packet);
	void RecvPacket();
	void MergePacket(int recv_bytes, char* recv_data);

	//getters
	SOCKET GetSocket() const { return socket; }
	int GetId() const { return id; }

	//setters
	void SetSocket(SOCKET new_socket) { socket = new_socket; }
	void SetId(int new_id) { id = new_id; }

	bool SetUse(bool expected, bool desired);
};

