#pragma once
#include <WinSock2.h>
#include <MSWSock.h>
#include <vector>
#include <atomic>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include "SendBuffer.h"
#include "ConcurrentTaskQueue.h"
#include "define_packets.h"
#include "DBResult.h"
#include "enum_class.h"
#include "session_tasks.h"

// 이 값은 send 및 recv 전에 최종 검증에 사용된다. GQCS에 이 값을 등록하고 값을 send, recv 까지 계속 전달하면, 만약 재사용됐을 경우 등록 직전에 세션 값과 이 값을 비교하면 된다.
// 다르다면 재사용된 것이므로 작업만 취소하면 된다. 같다면 같은 세션이므로 이전의 모든 작업을 신뢰할 수 있다. 당연히 비교 및 작업 등록은 세션 뮤텍스가 필요하다.

struct RoomSnapshot {
	ModeState mode_state;
	int room_index;
};

class Session
{
	SOCKET socket_ = INVALID_SOCKET;
	IOOverlapped recv_over_;
	SendBufferPool send_buffer_pool_;
	//IServer* server_interface;
	SessionKey session_key_;
	int room_index_ = -1;
	int remaining_data_size_ = 0;
	std::atomic<int> io_pending_count_ = 0;
	ConcurrentTaskQueue<std::unique_ptr<SessionTask>> session_tasks_;
	// 로비와 방 단계가 동시에 실행돼도 한 세션의 작업 큐는 한 스레드만 소비하도록 한다.
	std::atomic<bool> is_processing_tasks_{ false };
	// 남은 데이터는 recv_over 버퍼에 들어 있으므로 추가로 만들 필요가 없음.

	// 송수신 등록은 공유 잠금으로 병렬 처리하고 closesocket은 단독 잠금으로 막아, 닫힌 소켓 번호가 새 연결에 재사용되는 경쟁을 방지한다.
	mutable std::shared_mutex socket_mutex_;
	mutable std::mutex session_mutex_;
	std::atomic<LifeState> life_state_ = LifeState::NONE;
	std::atomic<ModeState> mode_state_ = ModeState::NONE;
	DBResultLogin db_info_;

	std::vector<FriendInfo> friend_list_;

public:
	Session();

	bool InitSession(int session_index, std::uint64_t session_id, SOCKET new_socket);
	bool ApplyLoginResult(DBResultLogin* login_result);
	bool SendPacket(const char* packet, int packet_size, const HANDLE iocp_handle);
	bool RecvPacket(const HANDLE iocp_handle);
	void AddFriend(FriendInfo& new_friend);
	void RemoveFriend(const int target_id);
	void InitFriendList(std::vector<FriendInfo>& db_friend_list);
	DBResultLogin UpdateMaxScore(int max_score);
	DBResultLogin UpdateMatchRecord(bool is_winner);
	bool BeginDisconnect(SessionKey session_key);
	bool CompleteIO(SessionKey session_key);
	bool CompleteRoomRemoval(SessionKey session_key);
	void FinalizeDisconnect(SessionKey session_key);
	bool TryEnqueueTask(SessionKey session_key, std::unique_ptr<SessionTask> task);
	void DiscardTasks();
	std::size_t ClaimTaskCount();
	void RestoreClaimedTaskCount(std::size_t task_count);
	std::unique_ptr<SessionTask> DequeueTask();
	bool TryStartTaskProcessing();
	void CompleteTaskProcessing();

	// 데이터 레이스를 방지하기 위해 세션은 언제든지 수정될 수 있는 데이터에 대해 참조 반환을 하지 않는다
	//getters
	SOCKET GetSocket() const;
	IOOverlapped& GetRecvOver() { return recv_over_; }
	std::vector<FriendInfo> GetFriendList() const;
	//IOKey GetIOKey() const { return key; }
	SessionKey GetSessionKey() const;
	int GetRoomIndex() const;
	RoomSnapshot GetRoomSnapshot() const;
	int GetRemainingDataSize() const;
	LifeState GetLifeState() const { return life_state_.load(); }
	ModeState GetModeState() const { return mode_state_.load(); }
	DBResultLogin GetDBInfo() const;
	//std::string GetPrimaryKey() const { return login_id; }

	//setters
	bool MatchesSessionKey(SessionKey session_key) const;
	void AdjustRemainingDataSize(int data_size_delta);
	bool TrySetLobbyMode(SessionKey session_key);
	bool TrySetRoomMode(SessionKey session_key, int new_room_index);
	//void SetPrimaryKey(std::string val) { login_id = val; }
};
