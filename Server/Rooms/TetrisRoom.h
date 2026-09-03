#pragma once
#include <chrono>
#include <atomic>
#include <vector>
#include <array>
#include <span>
#include <string>
#include <cstdint>
#include <utility>
#include "ConcurrentTaskQueue.h"
#include "Player.h"
#include "Session.h"
#include "define_packets.h"

class IOCPServer;
class TickThread;

struct PlayerInputTask
{
	EventType event_type;
	int player_id;
	std::uint64_t play_generation;
};

struct RoomTask
{
	RoomTaskType task_type = RoomTaskType::NONE;
	SP<Session> session;
	int target_player_id = -1;
	int matching_max_player_count = -1;
};

struct PublicRoomInitData {
	int room_gen = -1;
	int room_index = -1;
	char max_player_count = -1;
	std::string room_name;
};

struct PrivateRoomInitData {
	int room_gen = -1;
	int room_index = -1;
	char max_player_count = -1;
	std::string room_name;
	std::string room_password;
};

struct RoomInfoSnapshot {
	RoomState room_state = RoomState::EMPTY;
	int room_gen = -1;
	int max_player_count = 0;
	int current_player_count = 0;
	std::string room_name;
	bool is_private = false;
};

// 외부에서 들어오는 방 변경 작업은 큐에 보관하고, 방 처리권을 얻은 스레드만 room_players를 변경한다.
class TetrisRoom
{
	friend class TickThread;

protected:
	IOCPServer* server_;
	std::atomic<RoomState> room_state_;
	std::atomic<RoomProcessState> processing_state_{ RoomProcessState::PROCESSING };
	ConcurrentTaskQueue<RoomTask> room_tasks_;
	ConcurrentTaskQueue<PlayerInputTask> play_tasks_;
	std::atomic<std::uint64_t> play_generation_{ 0 };
	std::vector<char> tetromino_spawn_list_;
	Position spawn_pos_{ 3, 0 };

	int room_index_;
	int room_gen_;
	std::string room_name_;
	std::string room_password_;
	char max_player_count_;
	std::atomic<char> current_player_count_{ 0 };

	// 크기가 다른 자식 방의 고정 배열을 부모 공통 로직에서 동일하게 처리하기 위해 배열 범위를 반환한다.
	virtual std::span<Player> GetRoomPlayers() = 0;
	bool InitHostSession(const SP<Session>& session);
	void ClearPlayTasks();
	bool IsPlayerInRoom(const SP<Session>& session) const;
	void BeginRoomDelete();
	void TryPostRoomDelete();
	void ProcessRoomTasks();
	virtual void ProcessSpecificRoomTask(const RoomTask& task) = 0;
	virtual void ProcessGameTick(std::chrono::steady_clock::time_point tick_time) = 0;

public:
	TetrisRoom(IOCPServer* server, PublicRoomInitData data);
	TetrisRoom(IOCPServer* server, PrivateRoomInitData data);
	virtual ~TetrisRoom();

	int GetRoomGen() const { return room_gen_; }
	int GetRoomIndex() const { return room_index_; }
	bool IsPrivate() const { return !room_password_.empty(); }
	int GetMaxPlayerCount() const { return static_cast<int>(max_player_count_); }
	int GetCurrentPlayerCount() const;
	RoomInfoSnapshot GetRoomInfoSnapshot();
	const std::string& GetRoomName() const { return room_name_; }
	const std::string& GetRoomPassword() const { return room_password_; }

	// 공통
	virtual void HandlePacket(char* packet, const SP<Session>& request_session);
	void ProcessRoomTick(std::chrono::steady_clock::time_point tick_time);
	virtual void RemovePlayer(const int player_id) = 0;
	virtual void SendCreateRoom(const SP<Session>& session) = 0;
	virtual bool AddHostSession(const SP<Session>& session);
	bool AddRoomTask(RoomTask task);
	void AddPlayTask(PlayerInputTask task);
	void CompleteRoomInitialization();
	void StoreRoomState(const RoomState new_state);
	bool TryChangeRoomState(RoomState expected, RoomState desired);
	void InitGame();
	
	void ClearGame();
	
	void AppendTetromino7Bag();
	bool SpawnTetromino(int player_id);
	void ClearRoom(); // 이제 재사용이 아니라 아예 없앨거라서 굳이 방이 비워진 상태를 관리할 필요는 없다. 나중에 없애면 될 듯
	int AppendTickPackets();
	//void SendAddRoom(Session* session);
	bool AppendMovePacket(Player& player, int move_type);
	void AddGarbageLines();
	void AddSpawnTasks();
	void ResetPlayerTickState();
	void BroadcastPackets();
	void Broadcast(char* packet, const HANDLE iocp_handle);
};
