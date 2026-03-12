#pragma once
#include <WinSock2.h>
#include <MSWSock.h>
#include <mutex>
#include "ExOverlapped.h"
#include "Atomic.h"
#include "define.h"
#include "DBResult.h"
#include "enum_class.h"

class Session
{
	SOCKET socket;
	IOOverlapped recv_over;
	//IServer* server_interface;
	int id = -1;
	int index = -1; // 생성 시 결정하고 건들지 않아도 되는 값인 것 같은데?
	int room_index = -1;
	int remain_data_size = 0;
	// 남은 데이터는 recv_over 버퍼에 들어 있으므로 추가로 만들 필요가 없음.

	Atomic<bool> disconnect_flag = false;
	Atomic<SESS_STATE> state = SESS_STATE::NONE;
	DBResultLogin info;
	std::mutex sess_mutex; // send 작업 도중 disconnect를 막기 위한 것, 제너레이션은 send 성공 이후 ~ iocp 결과 처리 사이에 발생한 disconnect에 의한 예외를 막기 위한 것
public:

	Session();

	void InitSession(int new_index, int new_id, SOCKET new_socket);
	void ClearSession();
	void InitDBInfo(DBResultLogin* info);
	void SendPacket(char* packet, const HANDLE iocp_handle);
	void SendBoundPacket(char* packet_buf, int data_size, const HANDLE iocp_handle);
	void RecvPacket(const HANDLE iocp_handle);
	
	short GetPacketSize(char* packet);

	//getters
	SOCKET GetSocket() const { return socket; }
	IOOverlapped& GetExOver() { return recv_over; }

	int GetId() const { return id; }
	int GetIndex() const { return index; }
	int GetRoomIndex() const { return room_index; }
	int GetRemainDataSize() const { return remain_data_size; }
	SESS_STATE GetState() const { return state.Load(); }
	DBResultLogin& GetInfo() { return info; }
	std::mutex& GetMutex(){ return sess_mutex; }
	//std::string GetPrimaryKey() const { return login_id; }

	//setters
	void SetIndex(int new_index) { index = new_index; }
	void SetRoomIndex(int new_room_id) { room_index = new_room_id; }
	void SetRemainDataSize(int new_data_size) { remain_data_size += new_data_size; }
	void StoreState(SESS_STATE new_state) { state.Store(new_state); }
	bool TryChangeState(SESS_STATE expected, SESS_STATE desired);
	void StoreDisconnectFlag(bool new_flag) { disconnect_flag.Store(new_flag); }
	bool TryChangeDisconnectFlag(bool expected, bool desired);
	//void SetPrimaryKey(std::string val) { login_id = val; }
};
