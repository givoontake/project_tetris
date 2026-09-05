#pragma once
#include <atomic>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>
#include "ConcurrentTaskQueue.h"
#include "GameThread.h"
#include "room_lifecycle_tasks.h"
#include "game_state.h"

class IOCPServer;
class Session;
class TetrisRoom;
struct GamePhaseContext;
class TimerThread;

class GameThreadManager
{
public:
    static constexpr int THREAD_COUNT = 12;
    static constexpr int MIN_AVAILABLE_THREAD_COUNT = THREAD_COUNT / 2;

private:
    IOCPServer& iocp_server_;
    std::mutex tick_mutex_;
    std::condition_variable tick_cv_;
    // 혼합 대기의 사전 기상에 쓰던 공유 시각이며, 현재는 타이머 내부에서만 다음 틱 시각을 관리한다.
    // std::atomic<std::chrono::steady_clock::duration::rep> next_tick_time_count_{ 0 };
    TickWaitPolicy tick_wait_policy_;
	ConcurrentTaskQueue<std::unique_ptr<RoomLifecycleTask>> lifecycle_tasks_;
	std::atomic<bool> is_lifecycle_processing_{ false };
    //bool tick_enable = false;
    std::vector<std::unique_ptr<GameThread>> thread_objects_;
    std::vector<std::thread> threads_;

    friend class GameThread;
    friend class TimerThread;

	int GetAvailableThreadCount() const;
	std::vector<GameThread*> SelectThreads();
	void ProcessTask(std::unique_ptr<RoomLifecycleTask> task);
	int CreatePublicRoom(char* packet, Session& session, SessionKey session_key);
	int CreatePrivateRoom(char* packet, Session& session, SessionKey session_key);
	void DeleteRoom(int room_index);
	int GenerateRoomGen();
	std::shared_ptr<TetrisRoom> FindRoomByGen(int room_gen);

public:
    GameThreadManager(IOCPServer& iocp_server, TickWaitPolicy tick_policy);

    void Start();
	bool StartGamePhase(long long tick_time_ms);
	void Enqueue(std::unique_ptr<RoomLifecycleTask> task);
	int TryJoinRoom(SessionKey session_key, int room_gen, const std::string& room_password, int matching_max_player_count = -1);
	void ProcessPacket(char* packet, Session& session);
    void Close();
    void Join();
};
