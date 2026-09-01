#pragma once
#include <WinSock2.h>
#include <MSWSock.h>
#include <vector>
#include <array>
#include <atomic>
#include <memory>
#include <mutex>
#include "ExOverlapped.h"
#include "define_packets.h"
#include "DBResult.h"
#include "enum_class.h"

// 이 값은 send 및 recv 전에 최종 검증에 사용된다. GQCS에 이 값을 등록하고 값을 send, recv 까지 계속 전달하면, 만약 재사용됐을 경우 등록 직전에 세션 값과 이 값을 비교하면 된다.
// 다르다면 재사용된 것이므로 작업만 취소하면 된다. 같다면 같은 세션이므로 이전의 모든 작업을 신뢰할 수 있다. 당연히 비교 및 작업 등록은 세션 뮤텍스가 필요하다.

struct RoomSnapShot {
	ModeState state;
	int room_index;
};

class Session
{
	SOCKET socket_;
	IOOverlapped recv_over_;
	//IServer* server_interface;
	SessionKey key_;
	int room_index_ = -1;
	int remain_data_size_ = 0;
	std::atomic<int> io_pending_count_ = 0;
	// 남은 데이터는 recv_over 버퍼에 들어 있으므로 추가로 만들 필요가 없음.

	mutable std::mutex sess_mutex_;
	std::atomic<LifeState> life_state_ = LifeState::NONE;
	std::atomic<ModeState> mode_state_ = ModeState::NONE;
	DBResultLogin db_info_;

	std::vector<FriendInfo> friend_list_;

public:
	Session();

	void InitSession(SOCKET new_socket);
	bool InitDBInfo(DBResultLogin* info);
	void SendPacket(char* packet, const HANDLE iocp_handle);
	void SendBoundPacket(char* packet_buf, int data_size, const HANDLE iocp_handle);
	void RecvPacket(const HANDLE iocp_handle);
	void AddFriend(FriendInfo& new_friend);
	void DeleteFriend(const int target_id);
	void InitFriendList(std::vector<FriendInfo>& db_friend_list);
	DBResultLogin UpdateMaxScore(int max_score);
	DBResultLogin UpdateMatchRecord(bool is_winner);
	bool TryAddPending();
	void ReducePending();
	bool IsDisconnectable();
	bool BeginDeactivate();
	bool TryDeactivate();

	// 데이터 레이스를 방지하기 위해 세션은 언제든지 수정될 수 있는 데이터에 대해 참조 반환을 하지 않는다
	//getters
	SOCKET GetSocket() const;
	IOOverlapped& GetExOver() { return recv_over_; }
	std::vector<FriendInfo> GetFriendList() const;
	std::mutex& GetMutex() { return sess_mutex_; }

	//IOKey GetIOKey() const { return key; }
	SessionKey GetSessionKey() const;
	int GetRoomIndex() const;
	RoomSnapShot GetRoomSnapShot() const;
	int GetRemainDataSize() const;
	LifeState GetLifeState() const { return life_state_.load(); }
	ModeState GetState() const { return mode_state_.load(); }
	ModeState GetModeState() const { return mode_state_.load(); }
	DBResultLogin GetDBInfo() const;
	//std::string GetPrimaryKey() const { return login_id; }

	//setters
	void SetIndex(int new_index);
	void AddDataSize(int new_data_size);
	void StoreLifeState(LifeState new_state);
	void StoreState(ModeState new_state);
	void SetRoomSnapShot(ModeState new_state, int new_room_index);
	bool TrySetRoomMode(int new_room_index);
	bool TryChangeLifeState(LifeState expected, LifeState desired);
	bool TryChangeState(ModeState expected, ModeState desired);
	//void SetPrimaryKey(std::string val) { login_id = val; }
};
