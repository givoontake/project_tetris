#pragma once
#include <WinSock2.h>
#include <MSWSock.h>
#include <mutex>
#include <atomic>
#include "ExOverlapped.h"

enum SESSION_STATE {NONE, LOGIN, LOBBY}; // LOGIN은 서버에 연결은 되었지만 아직 로그인 확인을 받지 못한 상태, 받으면 LOBBY

class Session
{
	SOCKET socket;
	ExOverlapped recv_over;
	std::mutex session_mutex;
	int index = -1;
	int id = -1;

	// 남은 데이터는 recv_over 버퍼에 들어 있으므로 추가로 만들 필요가 없음

	std::atomic<SESSION_STATE> s_state = NONE;
public:
	int remain_data_size = 0;
	long long last_time; // 지연시간 파악에 사용
	std::atomic<long long> last_send_time = 0; // 타이머 스레드의 자동 send에 사용
public:
	Session();

	void SendPacket(char* packet, HANDLE iocp_handle);
	void RecvPacket(HANDLE iocp_handle);
	//void ProcessPacket(int recv_bytes, int key, BOOL res);
	short GetPacketSize(char* packet);
	void ClearSession();

	//getters
	SOCKET GetSocket() const { return socket; }
	ExOverlapped& GetExOver() { return recv_over; };
	int GetIndex() const { return index; }
	int GetId() const { return id; }
	int GetRemainDataSize() const { return remain_data_size; }
	SESSION_STATE GetState() const { return s_state; }

	//setters
	void SetSocket(SOCKET new_socket) { socket = new_socket; }
	void SetIndex(int new_index) { index = new_index; }
	void SetId(int new_id) { id = new_id; }
	void SetRemainDataSize(int new_size) { remain_data_size += new_size; }

	bool SetState(SESSION_STATE expected, SESSION_STATE desired);
	void SetState(SESSION_STATE desired);
};

