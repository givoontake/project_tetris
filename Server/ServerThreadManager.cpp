#include "ServerThreadManager.h"
#include <algorithm>
#include <chrono>
#include <thread>
#include <vector>
#include <Psapi.h>

#pragma comment(lib, "Psapi.lib")

namespace
{
	constexpr ULONG SYSTEM_PROCESSOR_PERFORMANCE_INFORMATION_CLASS = 8;

	struct SystemProcessorPerformanceInfo {
		LARGE_INTEGER IdleTime;
		LARGE_INTEGER KernelTime;
		LARGE_INTEGER UserTime;
		LARGE_INTEGER DpcTime;
		LARGE_INTEGER InterruptTime;
		ULONG InterruptCount;
	};

	using NtQuerySystemInformationFunc = LONG(WINAPI*)(ULONG, PVOID, ULONG, PULONG);

	NtQuerySystemInformationFunc GetNtQuerySystemInformation()
	{
		static NtQuerySystemInformationFunc func = [] {
			HMODULE module = GetModuleHandleW(L"ntdll.dll");
			if (!module) module = LoadLibraryW(L"ntdll.dll");
			if (!module) return NtQuerySystemInformationFunc{};
			return reinterpret_cast<NtQuerySystemInformationFunc>(GetProcAddress(module, "NtQuerySystemInformation"));
			}();
		return func;
	}
}

ServerThreadManager::ServerThreadManager(IOCPServer& server)
	: iocp_server(server), server_metrics(server.GetServerMetrics()), view_session(server.GetViewSession())
{
}

void ServerThreadManager::WorkerThread()
{
	iocp_server.ProcessGQCS();
}

void ServerThreadManager::TickWorkerThread()
{
	int last_tick = 0;
	int tick_worker_index = tick_worker_index_counter.fetch_add(1);

	while (iocp_server.GetRunning())
	{
		std::unique_lock<std::mutex> lock(g_tick_mutex);

		// 람다 함수가 참을 반환해야 다음 코드가 진행된다. notify 관련 함수가 호출되면 조건 검사를 다시 수행한다.
		g_tick_cv.wait(lock, [&] { // 전달한 lock으로 람다 내부 범위를 lock, 조건 검사하고 바로 unlock한다.
			if (!iocp_server.GetRunning()) return true; // 종료 신호가 오면 바로 빠져나오기
			return g_tick_counter.load() != last_tick;
			});

		int current_tick = g_tick_counter.load(); // 틱이 밀려 있어도 새로운 처리를 전달받은 시점에 한 번에 처리
		lock.unlock();

		while (true) {
			int room_index = room_index_counter.fetch_add(1);
			if (room_index >= MAX_ROOM) break;

			auto room = iocp_server.GetRoom(room_index);
			if (room) {
				room->ProcessPlayTasks();
			}
		}

		if (tick_worker_index >= 0 && tick_worker_index < MAX_TICK_WORKER_METRICS) {
			server_metrics.tick_worker_processed_ticks[tick_worker_index].fetch_add(1);
		}
		last_tick = current_tick;
	}
}

void ServerThreadManager::TimerThread()
{
	using clock = std::chrono::steady_clock;

	auto next_tick = clock::now();

	while (iocp_server.GetRunning())
	{
		// 다음 틱 시각 계산 후 그때까지 잠자기 cpu 내의 하드웨어 타이머를 통해 타이머 인터럽트가 발생하면 깨어나므로, 대기 중 CPU를 소모하지 않는다.
		next_tick += std::chrono::milliseconds(FRAME_TIME);
		std::this_thread::sleep_until(next_tick);

		room_index_counter.store(0);
		int next_tick_count = g_tick_counter.fetch_add(1) + 1; // 새 틱 발생
		if (next_tick_count >= TICK_COUNTER_RESET_VALUE) {
			g_tick_counter.store(0);
		}

		// 새 틱이 생겼으니 TickWorker 들을 깨운다.
		g_tick_cv.notify_all();
		metrics_counter.fetch_add(1);
		metrics_cv.notify_one();
	}
}

void ServerThreadManager::MetricsThread()
{
	using clock = std::chrono::steady_clock;

	int last_metrics_count = metrics_counter.load();
	UpdateProcessorMetrics();
	auto last_metrics_time = clock::now();

	while (iocp_server.GetRunning())
	{
		std::unique_lock<std::mutex> lock(metrics_mutex);
		metrics_cv.wait(lock, [&] {
			if (!iocp_server.GetRunning()) return true;
			return metrics_counter.load() != last_metrics_count;
			});

		last_metrics_count = metrics_counter.load();
		lock.unlock();

		auto now_time = clock::now();
		if (now_time - last_metrics_time >= std::chrono::seconds(1)) {
			auto metrics_elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(now_time - last_metrics_time).count();
			UpdateProcessorMetrics();
			SendViewMetrics(static_cast<std::uint64_t>(metrics_elapsed_ms));
			last_metrics_time = now_time;
		}
	}
}

void ServerThreadManager::DBThread()
{
	iocp_server.GetDB().Run();
}

void ServerThreadManager::UpdateProcessorMetrics()
{
	auto nt_query = GetNtQuerySystemInformation();
	if (!nt_query) return;

	DWORD processor_count = GetActiveProcessorCount(ALL_PROCESSOR_GROUPS);
	if (processor_count == 0) return;

	std::vector<SystemProcessorPerformanceInfo> processor_infos(processor_count);
	ULONG return_length = 0;
	LONG result = nt_query(
		SYSTEM_PROCESSOR_PERFORMANCE_INFORMATION_CLASS,
		processor_infos.data(),
		static_cast<ULONG>(processor_infos.size() * sizeof(SystemProcessorPerformanceInfo)),
		&return_length);
	if (result < 0) return;

	std::size_t reported_count = return_length / sizeof(SystemProcessorPerformanceInfo);
	if (reported_count == 0) reported_count = processor_infos.size();
	std::size_t update_count = std::min<std::size_t>(reported_count, MAX_LOGICAL_PROCESSOR_METRICS);

	server_metrics.logical_processor_count.store(static_cast<std::uint64_t>(update_count));
	for (std::size_t i = 0; i < update_count; ++i) {
		std::uint64_t idle = static_cast<std::uint64_t>(processor_infos[i].IdleTime.QuadPart);
		std::uint64_t kernel = static_cast<std::uint64_t>(processor_infos[i].KernelTime.QuadPart);
		std::uint64_t user = static_cast<std::uint64_t>(processor_infos[i].UserTime.QuadPart);

		if (has_prev_processor_times) {
			std::uint64_t idle_delta = idle - prev_processor_times[i].idle;
			std::uint64_t kernel_delta = kernel - prev_processor_times[i].kernel;
			std::uint64_t user_delta = user - prev_processor_times[i].user;
			std::uint64_t total_delta = kernel_delta + user_delta;
			std::uint64_t busy_delta = total_delta > idle_delta ? total_delta - idle_delta : 0;
			std::uint64_t usage = total_delta == 0 ? 0 : (busy_delta * 100) / total_delta;
			server_metrics.logical_processor_usage[i].store(std::min<std::uint64_t>(usage, 100));
		}
		else {
			server_metrics.logical_processor_usage[i].store(0);
		}

		prev_processor_times[i].idle = idle;
		prev_processor_times[i].kernel = kernel;
		prev_processor_times[i].user = user;
	}

	for (std::size_t i = update_count; i < MAX_LOGICAL_PROCESSOR_METRICS; ++i) {
		server_metrics.logical_processor_usage[i].store(0);
	}
	has_prev_processor_times = true;
}

void ServerThreadManager::SendViewMetrics(std::uint64_t metrics_elapsed_ms)
{
	if (!view_session.IsActive()) return;
	if (metrics_elapsed_ms == 0) metrics_elapsed_ms = 1;

	PROCESS_MEMORY_COUNTERS_EX memory_counters{};
	memory_counters.cb = sizeof(memory_counters);
	if (GetProcessMemoryInfo(GetCurrentProcess(), reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&memory_counters), sizeof(memory_counters))) {
		server_metrics.server_memory_bytes.store(static_cast<std::uint64_t>(memory_counters.PrivateUsage));
	}

	std::uint64_t latency_sample_count = server_metrics.latency_sample_count.load();
	S2V_SERVER_METRICS_PACKET packet{};
	packet.header.size = static_cast<std::uint16_t>(sizeof(packet));
	packet.header.type = S2V_SERVER_METRICS;
	packet.metrics.current_connected_users = server_metrics.current_connected_users.load();
	packet.metrics.total_connected_users = server_metrics.total_connected_users.load();
	packet.metrics.current_latency_ms = server_metrics.current_latency_ms.load();
	packet.metrics.average_latency_ms = latency_sample_count == 0 ? 0 : server_metrics.latency_total_ms.load() / latency_sample_count;
	packet.metrics.max_latency_ms = server_metrics.max_latency_ms.load();
	packet.metrics.completed_pending_total = server_metrics.completed_pending_total.load();
	packet.metrics.current_pending_count = server_metrics.current_pending_count.load();
	packet.metrics.active_room_count = server_metrics.active_room_count.load();
	packet.metrics.server_memory_bytes = server_metrics.server_memory_bytes.load();
	packet.metrics.created_thread_count = server_metrics.created_thread_count.load();
	packet.metrics.logical_processor_count = server_metrics.logical_processor_count.load();
	packet.metrics.tick_worker_count = MAX_TICK_WORKERS;
	for (int i = 0; i < MAX_LOGICAL_PROCESSOR_METRICS; ++i) {
		packet.metrics.logical_processor_usage[i] = server_metrics.logical_processor_usage[i].load();
	}
	for (int i = 0; i < MAX_TICK_WORKER_METRICS; ++i) {
		if (i < MAX_TICK_WORKERS) {
			std::uint64_t tick_count = server_metrics.tick_worker_processed_ticks[i].exchange(0);
			packet.metrics.tick_worker_ticks_per_second[i] = tick_count * 1000 / metrics_elapsed_ms;
		}
		else {
			packet.metrics.tick_worker_ticks_per_second[i] = 0;
		}
	}

	view_session.SendPacket(reinterpret_cast<char*>(&packet), packet.header.size, iocp_server.GetHandle());
}
