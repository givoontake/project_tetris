#pragma once
#include <WinSock2.h>
#include <MSWSock.h>
#include <mutex>
#include "ExOverlapped.h"
#include "Atomic.h"

class Session
{
	SOCKET socket;
	ExOverlapped recv_over;
	std::mutex session_mutex;
	//IServer* server_interface;
	int id = -1;
	int index = -1;
	int room_index = -1;
	int remain_data_size = 0;
	// 남은 데이터는 recv_over 버퍼에 들어 있으므로 추가로 만들 필요가 없음.

	Atomic<USER_STATE> state = NONE;
public:

	Session(); // 인자로 IPacketHandler를 받을 때 자식 클래스 Packethandler를 받는다->업캐스팅, 자식에서 재정의한 가상함수만 사용 가능하다.

	void InitSession(int new_index, int new_id, SOCKET new_socket);
	void SendPacket(char* packet, const HANDLE iocp_handle);
	void SendBoundPacket(char* packet, int data_size, const HANDLE iocp_handle);
	void RecvPacket(const HANDLE iocp_handle);
	
	short GetPacketSize(char* packet);

	//getters
	SOCKET GetSocket() const { return socket; }
	ExOverlapped& GetExOver() { return recv_over; }

	int GetId() const { return id; }
	//int GetIndex() const { return index; }
	int GetRoomIndex() const { return room_index; }
	int GetRemainDataSize() const { return remain_data_size; }
	USER_STATE GetState() const { return state.GetSelf(); }

	//setters
	void SetIndex(int new_index) { index = new_index; }
	void SetRoomIndex(int new_room_id) { room_index = new_room_id; }
	void SetRemainDataSize(int new_data_size) { remain_data_size += new_data_size; }
	void SetState(USER_STATE new_state) { state = new_state; } // 흠..?
	bool SetState(USER_STATE expected, USER_STATE desired);
	
};
