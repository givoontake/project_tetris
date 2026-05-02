#pragma once
#include <atomic>
#include <condition_variable>
#include <mutex>
#include "IOCPServer.h"
#include "ViewSession.h"

class ServerThreadManager
{
	IOCPServer& iocp_server;
	ServerMetrics& server_metrics;
	ViewSession& view_session;
	std::mutex g_tick_mutex;
	std::condition_variable g_tick_cv;
	std::atomic<int> g_tick_counter = 0;
	std::atomic<int> room_index_counter = 0;
	//bool tick_enable = false;

	static constexpr int TICK_COUNTER_RESET_VALUE = 100000000;

public:
	ServerThreadManager(IOCPServer& server);

	void WorkerThread();
	void TickWorkerThread(int num);
	void TimerThread();
	void DBThread();

private:
	void SendViewMetrics();
};
