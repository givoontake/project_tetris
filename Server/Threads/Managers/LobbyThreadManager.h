#pragma once
#include <array>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>
#include "ConcurrentTaskQueue.h"
#include "Lobby/LobbySession.h"
#include "LobbyThread.h"
#include "lobby_tasks.h"

class IOCPServer;

class LobbyThreadManager
{
public:
	static constexpr int THREAD_COUNT = 2;

private:
	IOCPServer& iocp_server_;
	std::mutex lobby_mutex_;
	std::condition_variable lobby_cv_;
	ConcurrentTaskQueue<std::unique_ptr<LobbyTask>> lifecycle_tasks_;
	std::array<LobbySession, MAX_PLAYER_COUNT> lobby_sessions_;
	std::vector<std::unique_ptr<LobbyThread>> thread_objects_;
	std::vector<std::thread> threads_;

	friend class LobbyThread;

public:
	explicit LobbyThreadManager(IOCPServer& iocp_server);

	void Start();
	void StartLobbyPhase();
	void Enqueue(std::unique_ptr<LobbyTask> task);
	LobbySession* GetLobbySession(int session_index);
	void Close();
	void Join();
};
