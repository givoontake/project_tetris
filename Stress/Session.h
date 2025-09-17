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

	// 남은 데이터는 recv_over 버퍼에 들어 있으므로 추가로 만들 필요가 없음

	std::atomic<bool> in_use = false;
public:
	int remain_data_size = 0;
	long long last_time; // 지연시간 파악에 사용
	std::atomic<long long> last_send_time; // 타이머 스레드의 자동 send에 사용
	
public:
	Session(IPacketHandler* p_handler);

	void SendPacket(char* packet);
	void RecvPacket();
	void ProcessPacket(int recv_bytes, int key, BOOL res);
	short GetPacketSize(char* packet);

	//getters
	SOCKET GetSocket() const { return socket; }
	int GetId() const { return id; }
	bool GetUse() const { return in_use; }

	//setters
	void SetSocket(SOCKET new_socket) { socket = new_socket; }
	void SetId(int new_id) { id = new_id; }

	bool SetUse(bool expected, bool desired);
	void SetUse(bool desired);
};

