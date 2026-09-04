#pragma once
#include <chrono>
#include <atomic>
#include <vector>
#include <array>
#include <span>
#include <string>
#include <cstdint>
#include <utility>
#include <optional>
#include "ConcurrentTaskQueue.h"
#include "Player.h"
#include "Session.h"
#include "define_packets.h"

class IOCPServer;
class GameThread;

struct PlayerInputTask
{
	EventType event_type;
	int player_id;
	std::uint64_t play_generation;
};

struct RoomTask
{
	RoomTaskType task_type = RoomTaskType::NONE;
	SessionKey session_key; // 세션이 먼저 정리돼도 지연된 방 작업이 같은 세대의 플레이어만 처리하도록 보관한다.
	int target_player_id = -1;
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
	friend class GameThread;
	friend class IOCPServer;

protected:
	IOCPServer* server_;
	std::atomic<RoomState> room_state_;
	std::atomic<RoomProcessState> processing_state_{ RoomProcessState::PROCESSING };
	ConcurrentTaskQueue<RoomTask> room_tasks_;
	ConcurrentTaskQueue<PlayerInputTask> play_tasks_;
	std::optional<RoomTask> pending_room_task_;
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
	Session* FindSession(const Player& player) const;
	Player* FindPlayer(SessionKey session_key);
	SessionKey ClearPlayer(Player& player);
	bool InitHostSession(Session* session, SessionKey session_key);
	bool RequestLobbyTransition(SessionKey session_key, RoomExitType exit_type = RoomExitType::LEAVE);
	virtual void CompletePlayerRemoval(SessionKey session_key, RoomExitType exit_type) = 0;
	virtual void HandlePlayerReactivated() {}
	void ClearPlayTasks();
	bool IsPlayerInRoom(Session* session) const;
	void BeginRoomDelete();
	void TryPostRoomDelete();
	void ProcessSessionTasks();
	void ProcessRoomTasks();
	bool TryProcessRoomTask(const RoomTask& task);
	virtual bool ProcessSpecificRoomTask(const RoomTask& task) = 0;
	virtual void ProcessGameTick(long long tick_time_ms) = 0;

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
	virtual void HandlePacket(char* packet, Session* request_session);
	void ProcessRoomTick(long long tick_time_ms);
	virtual void RemovePlayer(SessionKey session_key) = 0;
	virtual void SendCreateRoom(Session* session) = 0;
	virtual bool AddHostSession(Session* session, SessionKey session_key);
	bool ActivatePlayer(SessionKey session_key);
	void CompleteLobbyTransition(SessionKey session_key, int result, RoomExitType exit_type);
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
	SessionKey AppendTickPackets();
	//void SendAddRoom(Session* session);
	bool AppendMovePacket(Player& player, int move_type);
	void AddGarbageLines();
	void AddSpawnTasks();
	void ResetPlayerTickState();
	void BroadcastPackets();
	void Broadcast(char* packet, const HANDLE iocp_handle);
};
