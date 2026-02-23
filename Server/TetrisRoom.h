#pragma once
#include <vector>
#include <array>
#include <mutex>
#include "RoomSession.h"
#include "Session.h"
#include "define.h"
#include "packetType.h"
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

	//std::mutex room_mutex;
	
public:
	TetrisRoom(IOCPServer* server, Session* session, char max_user, char room_name[MAX_ROOM_NAME], char room_password[MAX_ROOM_PASSWORD]);
	TetrisRoom(IOCPServer* server, Session* session, char max_user, char room_name[MAX_ROOM_NAME]);
	virtual ~TetrisRoom();

	ROOM_STATE GetRoomState() const { return room_state.GetSelf(); }
	Tasks& GetTasks() { return tasks; }

	// 공통(오버라이드)
	virtual void HandlePacket(char* packet, Session* request_session) = 0;
	virtual void BoundPackets(RoomSession& r_session, std::vector<TaskType>& tasks) = 0;
	virtual void DeleteUser(const int id) = 0;	
	// 공통
	void SetRoomIndex(const int val);
	void SetRoomId(const int val);
	void SetRoomState(const ROOM_STATE new_state); // 방 상태 변경은 딱히 동시접근할 일이 없어보임
	void InitGame();
	
	void ClearGame();
	void ProcessPlayTasks();
	void Add7BagTetrominoList();
	bool SetNewTetromino(int id);
	void ClearRoom(); // 이제 재사용이 아니라 아예 없앨거라서 굳이 방이 비워진 상태를 관리할 필요는 없다. 나중에 없애면 될 듯
	void UpdateTick();
	void SendAddRoom(Session* session);
	void MakeMovePacket(RoomSession& r_session, int move_type);
	void ClearEventsInTick();
	void BroadcastTickData();
	void Broadcast(char* packet, const HANDLE iocp_handle);

	// 싱글 전용
	void CalculateScore(RoomSession& r_session, int clear_line_count);
	void RequestUpdateScore(RoomSession& r_session);

};
