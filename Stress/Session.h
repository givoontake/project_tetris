#pragma once
#include <WinSock2.h>
#include <MSWSock.h>
#include <mutex>
#include <atomic>
#include <cstdint>
#include "ExOverlapped.h"

enum SESSION_STATE { NONE, CONNECTING, LOGIN, LOBBY, ENTER_ROOM, PLAYING, DISCONNECTING };

class Session
{
	SOCKET socket = INVALID_SOCKET;
	ExOverlapped recv_over;
	std::mutex session_mutex;
	int index = -1;
	int id = -1;
	std::atomic<SESSION_STATE> s_state = NONE;

public:
	int remain_data_size = 0;
	long long last_time = -1;
	std::atomic<long long> login_send_time = -1;
	std::atomic<long long> last_send_time = -1;
	std::atomic<std::uint32_t> move_sequence = 0;

public:
	Session();

	void SendPacket(char* packet, HANDLE iocp_handle);
	void RecvPacket(HANDLE iocp_handle);
	std::uint16_t GetPacketSize(char* packet);

	void InitSession();
	void ClearSession();

	SOCKET GetSocket() const { return socket; }
	ExOverlapped& GetExOver() { return recv_over; };
	int GetIndex() const { return index; }
	int GetId() const { return id; }
	int GetRemainDataSize() const { return remain_data_size; }
	SESSION_STATE GetState() const { return s_state.load(); }

	void SetSocket(SOCKET new_socket) { socket = new_socket; }
	void SetIndex(int new_index) { index = new_index; }
	void SetId(int new_id) { id = new_id; }
	void SetRemainDataSize(int new_size) { remain_data_size += new_size; }

	bool SetState(SESSION_STATE expected, SESSION_STATE desired);
	void SetState(SESSION_STATE desired);
};
