#pragma once
#include <vector>
#include <array>
#include <mutex>
#include "RoomSession.h"
#include "Session.h"
#include "define.h"
#include "packetType.h"
#include "RoomPacketHandler.h"
#include "IOCPServer.h"

enum ROOM_STATE {EMPTY, WAIT, PLAY};

struct TaskInfo
{
	int type;
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
	//std::array<RoomSession*, MAX_USER>& users;
	std::vector<RoomSession> room_users; // 아토믹 변수는 복사가 안돼서..
	RoomPacketHandler room_handler;
	IOCPServer* server;
	Atomic<ROOM_STATE> room_state;
	Tasks tasks;
	std::vector<char> tetromino_spawn_list;

	int host_id;
	int room_id;
	char room_name[MAX_ROOM_NAME];
	bool is_password;
	char room_password[MAX_ROOM_PASSWORD];
	char max_user;

	//std::mutex room_mutex;
	
public:
	TetrisRoom(IOCPServer* server);
	~TetrisRoom();

	ROOM_STATE GetRoomState() const { return room_state.GetSelf(); }
	RoomPacketHandler& GetRoomPacketHandler() { return room_handler; }
	Tasks& GetTasks() { return tasks; }

	void SetRoomId(const int room_index);
	void SetRoomState(const ROOM_STATE new_state); // 방 상태 변경은 딱히 동시접근할 일이 없어보임
	int FindNewHost(int delete_id); // 방장이 나갔을 때 새로운 방장 찾기

	void InitRoom(char* packet, Session* session);
	void AddUser(Session* new_session);
	void DeleteUser(const int id);
	void ReadyUser(int id);
	void KickUser(int id, int kick_user_id);
	void StartGame(const int id);
	void Broadcast(char* packet, const HANDLE iocp_handle);
	void SendToSelf(char* packet, Session* session);

	void InitGame();
	void ClearGame();
	void ProcessPlayTasks();
	void Add7BagTetrominoList();
	bool SetNewTetromino(int id);
	void CheckWinner();
	void ClearRoom();
	//void SendToSelf(char* packet, Session* session);
};
