#pragma once
#include <atomic>
#include <vector>
#include <array>
#include <string>
#include <cstdint>
#include <utility>
#include "ConcurrentTaskQueue.h"
#include "RoomSession.h"
#include "Session.h"
#include "define_packets.h"

constexpr int ADD_TIMEOUT = 1;
constexpr int DOWN_TIMEOUT = 2;

class IOCPServer;
class TickThread;

struct TaskInfo
{
	EVENT_TYPE type;
	int id;
	std::uint64_t play_generation;
};

struct RoomTaskInfo
{
	ROOM_TASK_TYPE type = ROOM_TASK_TYPE::NONE;
	SP<Session> session;
	int target_id = -1;
	int matching_max_user = -1;
};

struct OpenRoomInitData {
	int room_gen = -1;
	int room_index = -1;
	char max_user = -1;
	std::string room_name;
};

struct LockRoomInitData {
	int room_gen = -1;
	int room_index = -1;
	char max_user = -1;
	std::string room_name;
	std::string room_password;
};

struct RoomInfoSnapshot {
	ROOM_STATE room_state = ROOM_STATE::EMPTY;
	int room_gen = -1;
	int max_user = 0;
	int cur_user = 0;
	std::string room_name;
	bool is_private = false;
};

// 외부에서 들어오는 방 변경 작업은 큐에 보관하고, 방 처리권을 얻은 스레드만 room_users를 변경한다.
class TetrisRoom
{
	friend class TickThread;

protected:
	//std::array<RoomSession*, MAX_USER>& users;
	std::vector<RoomSession> room_users; // 아토믹 변수는 복사가 안돼서..
	IOCPServer* server;
	Atomic<ROOM_STATE> room_state;
	std::atomic<ROOM_PROCESS_STATE> processing_state{ ROOM_PROCESS_STATE::PROCESSING };
	ConcurrentTaskQueue<RoomTaskInfo> room_tasks;
	ConcurrentTaskQueue<TaskInfo> play_tasks;
	std::atomic<std::uint64_t> play_generation{ 0 };
	std::vector<char> tetromino_spawn_list;
	Position spawn_pos{ 3, 0 };

	int room_index;
	int room_gen;
	std::string room_name;
	std::string room_password;
	char max_user;
	std::atomic<char> cur_user{ 0 };

	bool InitHostSession(const SP<Session>& session);
	void ClearPlayTasks();
	bool IsRoomSession(const SP<Session>& session) const;
	void BeginRoomDelete();
	void TryPostRoomDelete();
	void ProcessRoomTasks();
	virtual void ProcessSpecificRoomTask(const RoomTaskInfo& task) = 0;
	virtual void ProcessGameTick() = 0;

public:
	TetrisRoom(IOCPServer* server, OpenRoomInitData data);
	TetrisRoom(IOCPServer* server, LockRoomInitData data);
	virtual ~TetrisRoom();

	ROOM_STATE GetRoomState() const { return room_state.Load(); }
	int GetRoomGen() const { return room_gen; }
	int GetRoomIndex() const { return room_index; }
	bool GetIsPrivate() const { return !room_password.empty(); }
	int GetMaxUser() const { return static_cast<int>(max_user); }
	int GetCurrentUser() const;
	RoomInfoSnapshot GetRoomInfoSnapshot();
	const std::string& GetRoomName() const { return room_name; }
	const std::string& GetRoomPassword() const { return room_password; }

	// 공통
	virtual void HandlePacket(char* packet, const SP<Session>& request_session);
	void ProcessPlayTasks();
	virtual void DeleteUser(const int id) = 0;	
	virtual void SendCreateRoom(const SP<Session>& session) = 0;
	virtual bool AddHostSession(const SP<Session>& session);
	bool AddRoomTask(RoomTaskInfo task);
	void AddPlayTask(TaskInfo task);
	void CompleteRoomInitialization();
	void SetRoomIndex(const int val);
	void SetRoomGen(const int val);
	void StoreRoomState(const ROOM_STATE new_state);
	bool TryChangeRoomState(ROOM_STATE expected, ROOM_STATE desired);
	void InitGame();
	
	void ClearGame();
	
	void Add7BagTetrominoList();
	bool SpawnTetromino(int id);
	void ClearRoom(); // 이제 재사용이 아니라 아예 없앨거라서 굳이 방이 비워진 상태를 관리할 필요는 없다. 나중에 없애면 될 듯
	void UpdateTick();
	int BoundPackets();
	//void SendAddRoom(Session* session);
	bool MakeMovePacket(RoomSession& r_session, int move_type);
	void AddGarbageLines();
	void AddSpawnTask();
	void ResetUsersTickData();
	void BroadcastTickDataForUsers();
	void Broadcast(char* packet, const HANDLE iocp_handle);
};
