#pragma once
#include <WinSock2.h>
#include <MSWSock.h>
#include <mutex>
#include "ExOverlapped.h"
#include "Atomic.h"
#include "define.h"
#include "DBResult.h"
#include "enum_class.h"

// 이 값은 send 및 recv 전에 최종 검증에 사용된다. GQCS에 이 값을 등록하고 값을 send, recv 까지 계속 전달하면, 만약 재사용됐을 경우 등록 직전에 세션 값과 이 값을 비교하면 된다.
// 다르다면 재사용된 것이므로 작업만 취소하면 된다. 같다면 같은 세션이므로 이전의 모든 작업을 신뢰할 수 있다. 당연히 비교 및 작업 등록은 세션 뮤텍스가 필요하다.

struct SessionKey { 
	int index = -1;
	int id = -1;
};

class Session
{
	SOCKET socket;
	IOOverlapped recv_over;
	//IServer* server_interface;
	SessionKey key;
	int room_index = -1;
	int remain_data_size = 0;
	// 남은 데이터는 recv_over 버퍼에 들어 있으므로 추가로 만들 필요가 없음.

	Atomic<bool> disconnect_flag = false;
	Atomic<SESS_STATE> state = SESS_STATE::NONE;
	DBResultLogin info;
	std::mutex sess_mutex; // send 작업 도중 disconnect를 막기 위한 것, 제너레이션은 send 성공 이후 ~ iocp 결과 처리 사이에 발생한 disconnect에 의한 예외를 막기 위한 것
public:
	Session();

	void InitSession(int new_id, SOCKET new_socket);
	void ClearSession();
	void InitDBInfo(DBResultLogin* info);
	void SendPacket(char* packet, const HANDLE iocp_handle);
	void SendPacket(int reqeust_sess_id, char* packet, const HANDLE iocp_handle);
	void SendBoundPacket(char* packet_buf, int data_size, const HANDLE iocp_handle);
	void RecvPacket(int reqeust_sess_id, const HANDLE iocp_handle);
	
	short GetPacketSize(char* packet);

	//getters
	SOCKET GetSocket() const { return socket; }
	IOOverlapped& GetExOver() { return recv_over; }

	//IOKey GetIOKey() const { return key; }
	SessionKey GetSessionKey() const { return key; }
	int GetRoomIndex() const { return room_index; }
	int GetRemainDataSize() const { return remain_data_size; }
	SESS_STATE GetState() const { return state.Load(); }
	DBResultLogin& GetInfo() { return info; }
	std::mutex& GetMutex(){ return sess_mutex; }
	//std::string GetPrimaryKey() const { return login_id; }

	//setters
	void SetId(int new_id) { key.id = new_id; }
	void SetIndex(int new_index) { key.index = new_index; }
	void SetRoomIndex(int new_room_id) { room_index = new_room_id; }
	void SetRemainDataSize(int new_data_size) { remain_data_size += new_data_size; }
	void StoreState(SESS_STATE new_state) { state.Store(new_state); }
	bool TryChangeState(SESS_STATE expected, SESS_STATE desired);
	void StoreDisconnectFlag(bool new_flag) { disconnect_flag.Store(new_flag); }
	bool TryChangeDisconnectFlag(bool expected, bool desired);
	//void SetPrimaryKey(std::string val) { login_id = val; }
};
