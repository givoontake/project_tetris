#include "ServerThreadManager.h"
#include <chrono>
#include <thread>
#include <Psapi.h>

#pragma comment(lib, "Psapi.lib")

ServerThreadManager::ServerThreadManager(IOCPServer& server)
	: iocp_server(server), server_metrics(server.GetServerMetrics()), view_session(server.GetViewSession())
{
}

void ServerThreadManager::WorkerThread()
{
	iocp_server.ProcessGQCS();
}

void ServerThreadManager::TickWorkerThread(int num)
{
	int last_tick = 0;

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

		last_tick = current_tick;
	}
}

void ServerThreadManager::TimerThread()
{
	using clock = std::chrono::steady_clock;

	auto next_tick = clock::now();
	auto next_view_metrics_time = clock::now() + std::chrono::seconds(1);

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

		auto now_time = clock::now();
		if (now_time >= next_view_metrics_time) {
			SendViewMetrics();
			do {
				next_view_metrics_time += std::chrono::seconds(1);
			} while (now_time >= next_view_metrics_time);
		}
	}
}

void ServerThreadManager::DBThread()
{
	iocp_server.GetDB().Run();
}

void ServerThreadManager::SendViewMetrics()
{
	if (!view_session.IsActive()) return;

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

	view_session.SendPacket(reinterpret_cast<char*>(&packet), packet.header.size, iocp_server.GetHandle());
}
