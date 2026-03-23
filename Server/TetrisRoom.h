#pragma once
#include <vector>
#include <array>
#include <mutex>
#include "RoomSession.h"
#include "Session.h"
#include "define.h"
#include "MQueue.h"

constexpr int ADD_TIMEOUT = 1;
constexpr int DOWN_TIMEOUT = 2;

class IOCPServer;

struct TaskInfo
{
	EVENT_TYPE type;
	int id;
};

//struct TaskQueue
//{
//	MQueue<TaskInfo> tasks;
//	//std::atomic<bool> access = false;
//};

struct Tasks
{
	MQueue<TaskInfo> task_queue;
	MQueue<TaskInfo> pending_queue;

	Tasks() {
		//task_queue.access.store(false);
		//pending_queue.access.store(true);
	}

	TaskInfo GetTask() { return task_queue.DeQ(); }

	void AddTask(const TaskInfo& task) { pending_queue.EnQ(task); }
	void SwapTask() { task_queue.Swap(pending_queue); }
	void Clear() {
		MQueue<TaskInfo> empty;
		task_queue.Swap(empty);
		pending_queue.Swap(empty);
	}
};

struct OpenRoomInitData {
	int room_id = -1;
	int room_index = -1;
	char max_user = -1;
	char room_name[MAX_ROOM_NAME];
};

struct LockRoomInitData {
	int room_id = -1;
	int room_index = -1;
	char max_user = -1;
	char room_name[MAX_ROOM_NAME];
	char room_password[MAX_ROOM_PASSWORD];
};

// 현재 룸 세션 내부에 session*를 유지중이라, 유저가 삭제되면 범위기반 스코프 접근 시 해당 참조의 세션이 널일 수 있고, 방이 삭제되어 버리면 범위 자체가 손상되어 범위기반 작업은 모두 뮤텍스로 묶어놓은 상태이다.
// 포인터가 아니라 다르게 관리한다면 이 문제를 좀 더 효율적으로 해결할 수 있을 것 같다.

// 룸 내부의 세션은 Disconnect 로직 및 방 내부 뮤텍스에 의해 100% 유효한 상태
class TetrisRoom
{
protected:
	//std::array<RoomSession*, MAX_USER>& users;
	std::vector<RoomSession> room_users; // 아토믹 변수는 복사가 안돼서..
	IOCPServer* server;
	Atomic<ROOM_STATE> room_state;
	Tasks tasks;
	std::vector<char> tetromino_spawn_list;
	Position spawn_pos{ 3, 0 };

	int room_index;
	int room_id;
	char room_name[MAX_ROOM_NAME];
	char* room_password;
	char max_user;
	char cur_user;

	std::mutex room_mutex;

public:
	TetrisRoom(IOCPServer* server, Session* session, OpenRoomInitData data);
	TetrisRoom(IOCPServer* server, Session* session, LockRoomInitData data);
	virtual ~TetrisRoom();

	ROOM_STATE GetRoomState() const { return room_state.Load(); }
	Tasks& GetTasks() { return tasks; }
	std::mutex& GetRoomMutex() { return room_mutex; }
	int GetRoomId() const { return room_id; }
	int GetRoomIndex() const { return room_index; }
	bool GetIsPrivate() const { return room_password ? true : false; }
	int GetMaxUser() const { return static_cast<int>(max_user); }
	int GetCurrentUser() const { return static_cast<int>(max_user); }
	const char* GetRoomName() const { return room_name; }

	// 공통(오버라이드)
	virtual void HandlePacket(char* packet, Session* request_session) = 0;
	virtual void ProcessPlayTasks() = 0;
	virtual void DeleteUser(const int id) = 0;	
	virtual void SendCreateRoom(Session* session) = 0;
	// 공통
	void SetRoomIndex(const int val);
	void SetRoomId(const int val);
	void StoreRoomState(const ROOM_STATE new_state);
	bool TryChangeRoomState(ROOM_STATE expected, ROOM_STATE desired);
	void InitGame();
	
	void ClearGame();
	
	void Add7BagTetrominoList();
	bool SpawnTetromino(int id);
	void ClearRoom(); // 이제 재사용이 아니라 아예 없앨거라서 굳이 방이 비워진 상태를 관리할 필요는 없다. 나중에 없애면 될 듯
	void UpdateTick();
	void BoundPackets();
	//void SendAddRoom(Session* session);
	void MakeMovePacket(RoomSession& r_session, int move_type);
	void AddGarbageLines();
	void AddSpawnTask();
	void ResetUsersTickData();
	void BroadcastTickDataForUsers();
	void Broadcast(char* packet, const HANDLE iocp_handle);
};
