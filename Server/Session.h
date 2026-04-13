#pragma once
#include <WinSock2.h>
#include <MSWSock.h>
#include <mutex>
#include <vector>
#include <array>
#include "ExOverlapped.h"
#include "Atomic.h"
#include "define.h"
#include "DBResult.h"
#include "enum_class.h"

// 이 값은 send 및 recv 전에 최종 검증에 사용된다. GQCS에 이 값을 등록하고 값을 send, recv 까지 계속 전달하면, 만약 재사용됐을 경우 등록 직전에 세션 값과 이 값을 비교하면 된다.
// 다르다면 재사용된 것이므로 작업만 취소하면 된다. 같다면 같은 세션이므로 이전의 모든 작업을 신뢰할 수 있다. 당연히 비교 및 작업 등록은 세션 뮤텍스가 필요하다.

struct SessionKey { 
	int index = -1;
	int gen = -1;
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

	Atomic<bool> disconnect_flag = false; // 상태에 추가하려고 해도 DISCONNECT 전에 어떤 상태인지 알 수가 없어서 CAS가 불가능해 따로 추가한 값
	Atomic<SESS_STATE> state = SESS_STATE::NONE;
	DBResultLogin db_info;
	std::mutex sess_mutex;

	std::vector<FriendInfo> friend_list;

public:
	Session();

	void InitSession(int new_gen, SOCKET new_socket);
	void ClearSession();
	void InitDBInfo(DBResultLogin* info);
	void SendPacket(char* packet, const HANDLE iocp_handle);
	void SendPacket(int request_gen, char* packet, const HANDLE iocp_handle);
	void SendBoundPacket(char* packet_buf, int data_size, const HANDLE iocp_handle);
	void SendBoundPacket(int request_gen, char* packet_buf, int data_size, const HANDLE iocp_handle);
	void RecvPacket(int request_gen, const HANDLE iocp_handle);
	void AddFriend(FriendInfo& new_friend);
	void DeleteFriend(const int target_id);
	void InitFriendList(std::vector<FriendInfo>& db_friend_list);

	//getters
	SOCKET GetSocket() const { return socket; }
	IOOverlapped& GetExOver() { return recv_over; }
	std::vector<FriendInfo>& GetFriendList() { return friend_list; }

	//IOKey GetIOKey() const { return key; }
	SessionKey GetSessionKey() const { return key; }
	int GetRoomIndex() const { return room_index; }
	int GetRemainDataSize() const { return remain_data_size; }
	SESS_STATE GetState() const { return state.Load(); }
	DBResultLogin& GetDBInfo() { return db_info; }
	std::mutex& GetMutex(){ return sess_mutex; }
	//std::string GetPrimaryKey() const { return login_id; }

	//setters
	void SetGen(int new_gen) { key.gen = new_gen; }
	void SetIndex(int new_index) { key.index = new_index; }
	void SetRoomIndex(int new_room_index) { room_index = new_room_index; }
	void AddDataSize(int new_data_size) { remain_data_size += new_data_size; }
	void StoreState(SESS_STATE new_state) { state.Store(new_state); }
	bool TryChangeState(SESS_STATE expected, SESS_STATE desired);
	void StoreDisconnectFlag(bool new_flag) { disconnect_flag.Store(new_flag); }
	bool TryChangeDisconnectFlag(bool expected, bool desired);
	//void SetPrimaryKey(std::string val) { login_id = val; }
};
