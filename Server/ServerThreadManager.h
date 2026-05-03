#pragma once
#include <array>
#include <atomic>
#include <condition_variable>
#include <mutex>
#include "IOCPServer.h"
#include "ViewSession.h"

class ServerThreadManager
{
	struct ProcessorTimes {
		std::uint64_t idle = 0;
		std::uint64_t kernel = 0;
		std::uint64_t user = 0;
	};

	IOCPServer& iocp_server;
	ServerMetrics& server_metrics;
	ViewSession& view_session;
	std::mutex g_tick_mutex;
	std::condition_variable g_tick_cv;
	std::mutex metrics_mutex;
	std::condition_variable metrics_cv;
	std::atomic<int> g_tick_counter = 0;
	std::atomic<int> metrics_counter = 0;
	std::atomic<int> room_index_counter = 0;
	std::atomic<int> tick_worker_index_counter = 0;
	std::array<ProcessorTimes, MAX_LOGICAL_PROCESSOR_METRICS> prev_processor_times{};
	bool has_prev_processor_times = false;
	//bool tick_enable = false;

	static constexpr int TICK_COUNTER_RESET_VALUE = 100000000;

public:
	ServerThreadManager(IOCPServer& server);

	void WorkerThread();
	void TickWorkerThread();
	void TimerThread();
	void MetricsThread();
	void DBThread();

private:
	void UpdateProcessorMetrics();
	void SendViewMetrics(std::uint64_t metrics_elapsed_ms);
};
