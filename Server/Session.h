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

	std::atomic<bool> in_use = false;
	
public:
	int remain_data_size = 0;

	Session(IPacketHandler* p_handler); // 인자로 IPacketHandler를 받을 때 자식 클래스 Packethandler를 받는다->업캐스팅, 자식에서 재정의한 가상함수만 사용 가능하다.

	void SendPacket(char* packet);
	void RecvPacket();
	void MergePacket(int recv_bytes, char* recv_data);
	short GetPacketSize(char* packet);

	//getters
	SOCKET GetSocket() const { return socket; }
	int GetId() const { return id; }
	bool GetUse() const { return in_use; }

	//setters
	void SetSocket(SOCKET new_socket) { socket = new_socket; }
	void SetId(int new_id) { id = new_id; }

	bool SetUse(bool expected, bool desired);
};

