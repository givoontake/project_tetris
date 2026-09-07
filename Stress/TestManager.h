#pragma once
#include <WinSock2.h>
#include <MSWSock.h>
#include <array>
#include <atomic>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <string>
#include "LatencyRecorder.h"
#include "Session.h"

#pragma comment(lib, "WS2_32.lib")
#pragma comment(lib, "MSWSock.lib")

class TestManager
{
	HANDLE iocp_handle_ = nullptr;
	WSADATA wsa_data_{};
	SOCKADDR_IN server_address_{};
	LPFN_CONNECTEX connect_ex_ = nullptr;
	std::array<std::unique_ptr<Session>, MAX_USER> sessions_;
	std::array<std::atomic<RoomKey>, ROOM_COUNT> room_keys_{};
	std::array<std::atomic<long long>, ROOM_COUNT> room_join_ready_times_{};
	std::mutex connect_ex_mutex_;
	std::mutex connect_mutex_;
	std::condition_variable connect_cv_;
	std::mutex log_mutex_;
	std::atomic<int> connected_client_count_{ 0 };
	std::atomic<int> playing_client_count_{ 0 };
	std::atomic<int> ready_room_count_{ 0 };
	std::atomic<int> target_client_count_{ 0 };
	std::atomic<int> started_connect_count_{ 0 };
	std::atomic<int> connect_signal_count_{ 0 };
	std::atomic<bool> is_reconnect_enabled_{ true };
	LatencyRecorder latency_recorder_;

public:
	TestManager(const std::string& server_ip, std::string output_directory);
	~TestManager();

	void Start(int client_count);
	void ProcessConnect();
	void ProcessIO();
	void ProcessSend(int worker_index);
	bool IsMeasurementFinished() const { return latency_recorder_.IsFinished(); }

private:
	long long GetCurrentTimeMS() const;
	bool LoadConnectEx(SOCKET socket);
	bool ConnectToServer();
	int FindAvailableSessionIndex();
	void CompleteConnect(int session_index, ExOverlapped* connect_over);
	void ProcessRecvBuffer(int recv_bytes, int session_index);
	void HandlePacket(char* packet, int session_index);
	void SendTestLogin(int session_index);
	void SendCreateRoom(int host_index);
	void TryJoinRoom(int session_index);
	void SendReady(int session_index);
	void SendStart(int host_index);
	void SendMove(int session_index, long long send_time);
	void NotifyLoginComplete();
	void UpdateMeasurement();
	void Disconnect(int session_index);
};
