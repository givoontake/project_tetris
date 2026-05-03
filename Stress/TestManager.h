#pragma once
#include <WinSock2.h>
#include <MSWSock.h>
#include <array>
#include <atomic>
#include <condition_variable>
#include <memory>
#include <mutex>
#include "ExOverlapped.h"
#include "Session.h"
#include "MQueue.h"

#pragma comment(lib, "WS2_32.lib")
#pragma comment(lib, "MSWSock.lib")

class TestManager
{
	HANDLE iocp_handle = nullptr;
	WSADATA wsadata;
	SOCKADDR_IN server_addr;
	LPFN_CONNECTEX connect_ex = nullptr;
	SOCKET listen_socket = INVALID_SOCKET;
	SOCKET view_socket = INVALID_SOCKET;
	std::array<Session*, MAX_USER> sessions;
	std::mutex view_mutex;
	std::mutex log_mutex;
	std::mutex connect_mutex;
	std::condition_variable connect_cv;
	std::mutex latency_mutex;
	std::array<long long, STRESS_LATENCY_AVERAGE_COUNT> recent_latencies{};
	int recent_latency_index = 0;
	int recent_latency_count = 0;
	long long recent_latency_total = 0;
	std::atomic<long long> current_latency = 0;
	std::atomic<long long> recent_average_latency = 0;
	std::atomic<long long> max_latency = 0;
	std::atomic<long long> login_latency_total = 0;
	std::atomic<long long> login_latency_count = 0;
	std::atomic<long long> current_login_latency = 0;
	std::atomic<long long> average_login_latency = 0;
	std::atomic<long long> max_login_latency = 0;
	std::atomic<int> playing_client = 0;
	std::atomic<long long> last_print_time = 0;
	std::atomic<long long> last_connect_time = 0;
	std::atomic<int> target_connect_count = 0;
	std::atomic<int> started_connect_count = 0;
	std::atomic<int> connect_signal_count = 0;
	std::atomic<bool> connect_enabled = true;

public:
	std::atomic<int> connected_client = 0;
	char test_message[MAX_MESSAGE_SIZE]{};
	std::atomic<long long> delay = 0;

public:
	TestManager();
	~TestManager();

	bool StartConnectSessions(int session_count);
	bool ConnectToServer();
	void ProcessConnectThread();
	void ProcessGQCS();
	void ProcessSend();
	int GetClientIndex();
	void SetTestMessege(int message_size);
	void ProcessPacket(int recv_bytes, int user_index);
	void HandlePacket(char* packet, int user_index);
	void Disconnect(int session_index);
	void ProcessViewSocket();
	void ProcessViewControl(SOCKET socket);

	long long GetCurrentTimeMS();
	void AdjustSessionNumber(long long now_time, S2C_TEST_PACKET* p);

private:
	bool LoadConnectEx(SOCKET socket);
	void ProcessConnect(int user_index, ExOverlapped* connect_over);
	void SendTestLoginPacket(int user_index);
	void SendStressEnterMatchPacket(int user_index);
	void SendTestMovePacket(int user_index);
	void UpdateLatency(long long latency);
	void UpdateLoginLatency(int user_index);
	void NotifyLoginSuccess();
	void ReduceStartedConnectCount();
	bool DisconnectFrontClient();
	void ResetRecentLatency();
	long long GetConnectDelay(long long average_latency) const;
	void PrintMetrics(long long now_time);
	void SendViewMetrics();
	void SetConnectEnabled(bool enabled);
};
