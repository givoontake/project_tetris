#pragma once
#include <atomic>
#include <cstdint>

constexpr int VIEW_SESSION_ID = 99999;
constexpr const char* VIEW_SESSION_LOGIN_ID = "server";
constexpr const char* VIEW_SESSION_PASSWORD = "1234";
constexpr std::uint8_t S2V_SERVER_METRICS = 1;

struct ServerMetrics
{
	std::atomic<std::uint64_t> current_connected_users = 0;
	std::atomic<std::uint64_t> total_connected_users = 0;
	std::atomic<std::uint64_t> current_latency_ms = 0;
	std::atomic<std::uint64_t> latency_total_ms = 0;
	std::atomic<std::uint64_t> latency_sample_count = 0;
	std::atomic<std::uint64_t> max_latency_ms = 0;
	std::atomic<std::uint64_t> completed_pending_total = 0;
	std::atomic<std::int64_t> current_pending_count = 0;
	std::atomic<std::uint64_t> active_room_count = 0;
	std::atomic<std::uint64_t> server_memory_bytes = 0;
};

#pragma pack(push, 1)

struct MetricsPacketHeader
{
	std::uint16_t size;
	std::uint8_t type;
};

struct ServerMetricsSnapshot
{
	std::uint64_t current_connected_users;
	std::uint64_t total_connected_users;
	std::uint64_t current_latency_ms;
	std::uint64_t average_latency_ms;
	std::uint64_t max_latency_ms;
	std::uint64_t completed_pending_total;
	std::int64_t current_pending_count;
	std::uint64_t active_room_count;
	std::uint64_t server_memory_bytes;
};

struct S2V_SERVER_METRICS_PACKET
{
	MetricsPacketHeader header;
	ServerMetricsSnapshot metrics;
};

#pragma pack(pop)
