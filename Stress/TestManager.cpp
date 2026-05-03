#include <iostream>
#include <chrono>
#include <WS2tcpip.h>
#include <fstream>
#include <algorithm>
#include <cstdio>
#include <thread>
#include "TestManager.h"

namespace
{
	bool RecvExact(SOCKET socket, char* buffer, int size)
	{
		int received = 0;
		while (received < size) {
			int ret = recv(socket, buffer + received, size - received, 0);
			if (ret <= 0) return false;
			received += ret;
		}
		return true;
	}
}

TestManager::TestManager()
{
	for (int i = 0; i < MAX_USER; i++){
		sessions[i] = new Session();
	}
	WSAStartup(MAKEWORD(2, 2), &wsadata);

	memset(&server_addr, 0, sizeof(server_addr));
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(PORT_NUM);
	inet_pton(AF_INET, "127.0.0.1", &server_addr.sin_addr);

	iocp_handle = CreateIoCompletionPort(INVALID_HANDLE_VALUE, 0, 0, 0);
}

TestManager::~TestManager()
{
	if (listen_socket != INVALID_SOCKET) closesocket(listen_socket);
	if (view_socket != INVALID_SOCKET) closesocket(view_socket);
	for (auto& session : sessions) {
		if (!session) continue;
		if (session->GetSocket() != INVALID_SOCKET) closesocket(session->GetSocket());
		delete session;
		session = nullptr;
	}
	if (iocp_handle) CloseHandle(iocp_handle);
	WSACleanup();
}

void TestManager::ProcessViewSocket()
{
	listen_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (listen_socket == INVALID_SOCKET) {
		std::cout << "stress view socket failed: " << WSAGetLastError() << std::endl;
		return;
	}

	BOOL reuse_addr = TRUE;
	setsockopt(listen_socket, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&reuse_addr), sizeof(reuse_addr));

	SOCKADDR_IN view_addr{};
	view_addr.sin_family = AF_INET;
	view_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	view_addr.sin_port = htons(STRESS_VIEW_PORT);

	if (bind(listen_socket, reinterpret_cast<SOCKADDR*>(&view_addr), sizeof(view_addr)) == SOCKET_ERROR) {
		std::cout << "stress view bind failed: " << WSAGetLastError() << std::endl;
		closesocket(listen_socket);
		listen_socket = INVALID_SOCKET;
		return;
	}

	if (listen(listen_socket, 1) == SOCKET_ERROR) {
		std::cout << "stress view listen failed: " << WSAGetLastError() << std::endl;
		closesocket(listen_socket);
		listen_socket = INVALID_SOCKET;
		return;
	}

	std::cout << "stress view listen: 0.0.0.0:" << STRESS_VIEW_PORT << std::endl;
	while (true) {
		SOCKET accepted_socket = accept(listen_socket, nullptr, nullptr);
		if (accepted_socket == INVALID_SOCKET) {
			std::cout << "stress view accept failed: " << WSAGetLastError() << std::endl;
			return;
		}

		std::lock_guard<std::mutex> lock(view_mutex);
		if (view_socket != INVALID_SOCKET) closesocket(view_socket);
		view_socket = accepted_socket;
		std::cout << "stress view connected" << std::endl;
		std::thread(&TestManager::ProcessViewControl, this, accepted_socket).detach();
	}
}

void TestManager::ProcessViewControl(SOCKET socket)
{
	while (true) {
		PacketHeader header{};
		if (!RecvExact(socket, reinterpret_cast<char*>(&header), sizeof(header))) break;
		if (header.size < sizeof(PacketHeader) || header.size > BUF_SIZE) break;

		char payload[BUF_SIZE]{};
		int payload_size = header.size - static_cast<int>(sizeof(PacketHeader));
		if (payload_size > 0 && !RecvExact(socket, payload, payload_size)) break;

		if (header.type == V2S_STRESS_CONNECT_CONTROL && header.size == sizeof(V2S_STRESS_CONNECT_CONTROL_PACKET)) {
			V2S_STRESS_CONNECT_CONTROL_PACKET packet{};
			packet.header = header;
			memcpy(reinterpret_cast<char*>(&packet) + sizeof(PacketHeader), payload, payload_size);
			SetConnectEnabled(packet.connect_enabled != 0);
		}
	}

	std::lock_guard<std::mutex> lock(view_mutex);
	if (view_socket == socket) {
		closesocket(view_socket);
		view_socket = INVALID_SOCKET;
	}
}

long long TestManager::GetCurrentTimeMS()
{
	return std::chrono::duration_cast<std::chrono::milliseconds>(
		std::chrono::steady_clock::now().time_since_epoch()
	).count();
}

void TestManager::AdjustSessionNumber(long long now_time, S2C_TEST_PACKET* p)
{
	long long new_delay = now_time - p->last_time;
	delay.store(new_delay);
}

bool TestManager::LoadConnectEx(SOCKET socket)
{
	if (connect_ex) return true;
	GUID guid = WSAID_CONNECTEX;
	DWORD bytes = 0;
	return WSAIoctl(socket, SIO_GET_EXTENSION_FUNCTION_POINTER, &guid, sizeof(guid), &connect_ex, sizeof(connect_ex), &bytes, nullptr, nullptr) == 0;
}

bool TestManager::StartConnectSessions(int session_count)
{
	int count = session_count < MAX_USER ? session_count : MAX_USER;
	target_connect_count.store(count);
	started_connect_count.store(0);
	last_connect_time.store(0);
	connect_signal_count.store(count > 0 ? 1 : 0);
	connect_cv.notify_one();
	return true;
}

bool TestManager::ConnectToServer()
{
	int new_index = GetClientIndex();
	if (new_index == -1) return false;

	SOCKET client_socket = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, nullptr, 0, WSA_FLAG_OVERLAPPED);
	if (client_socket == INVALID_SOCKET) {
		sessions[new_index]->SetState(NONE);
		return false;
	}

	if (!LoadConnectEx(client_socket)) {
		closesocket(client_socket);
		sessions[new_index]->SetState(NONE);
		return false;
	}

	SOCKADDR_IN local_addr{};
	local_addr.sin_family = AF_INET;
	local_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	local_addr.sin_port = 0;
	if (bind(client_socket, reinterpret_cast<SOCKADDR*>(&local_addr), sizeof(local_addr)) == SOCKET_ERROR) {
		closesocket(client_socket);
		sessions[new_index]->SetState(NONE);
		return false;
	}

	sessions[new_index]->InitSession();
	sessions[new_index]->SetIndex(new_index);
	sessions[new_index]->SetId(new_index);
	sessions[new_index]->SetSocket(client_socket);
	sessions[new_index]->SetState(CONNECTING);

	CreateIoCompletionPort(reinterpret_cast<HANDLE>(client_socket), iocp_handle, new_index, 0);

	ExOverlapped* connect_over = new ExOverlapped;
	connect_over->SetExOverlapped(CONNECT);
	BOOL ret = connect_ex(client_socket, reinterpret_cast<SOCKADDR*>(&server_addr), sizeof(server_addr), nullptr, 0, nullptr, &connect_over->over);
	if (!ret && WSAGetLastError() != WSA_IO_PENDING) {
		delete connect_over;
		closesocket(client_socket);
		sessions[new_index]->SetState(NONE);
		return false;
	}

	return true;
}

long long TestManager::GetConnectDelay(long long average_latency) const
{
	if (average_latency <= 50) return 10;
	if (average_latency >= 100) return 2000;
	return 10 + ((average_latency - 50) * 1990) / 50;
}

void TestManager::NotifyLoginSuccess()
{
	connect_signal_count.fetch_add(1);
	connect_cv.notify_one();
}

void TestManager::ReduceStartedConnectCount()
{
	int expected = started_connect_count.load();
	while (expected > 0 && !started_connect_count.compare_exchange_weak(expected, expected - 1)) {
	}
}

bool TestManager::DisconnectFrontClient()
{
	for (int i = 0; i < STRESS_SESSION_COUNT; ++i) {
		SESSION_STATE state = sessions[i]->GetState();
		if (state == LOGIN || state == LOBBY || state == ENTER_ROOM || state == PLAYING) {
			Disconnect(i);
			return true;
		}
	}
	return false;
}

void TestManager::ResetRecentLatency()
{
	std::lock_guard<std::mutex> lock(latency_mutex);
	recent_latencies.fill(0);
	recent_latency_index = 0;
	recent_latency_count = 0;
	recent_latency_total = 0;
	recent_average_latency.store(0);
	delay.store(10);
}

void TestManager::ProcessConnectThread()
{
	while (true) {
		{
			std::unique_lock<std::mutex> lock(connect_mutex);
			connect_cv.wait_for(lock, std::chrono::milliseconds(100), [this] {
				return connect_signal_count.load() > 0;
				});
		}

		if (!connect_enabled.load()) continue;

		long long average_latency = recent_average_latency.load();
		if (average_latency > 100) {
			if (DisconnectFrontClient()) {
				std::this_thread::sleep_for(std::chrono::milliseconds(100));
			}
			else {
				ResetRecentLatency();
				connect_signal_count.fetch_add(1);
			}
			continue;
		}

		int target_count = target_connect_count.load();
		if (target_count <= 0) continue;
		if (started_connect_count.load() >= target_count) continue;
		if (connect_signal_count.load() <= 0 && started_connect_count.load() != 0) continue;

		long long connect_delay = GetConnectDelay(average_latency);
		delay.store(connect_delay);
		long long now_time = GetCurrentTimeMS();
		long long prev_connect_time = last_connect_time.load();
		if (prev_connect_time != 0 && now_time - prev_connect_time < connect_delay) {
			std::this_thread::sleep_for(std::chrono::milliseconds(connect_delay - (now_time - prev_connect_time)));
		}

		average_latency = recent_average_latency.load();
		if (!connect_enabled.load()) continue;
		if (average_latency > 100) continue;
		if (started_connect_count.load() >= target_count) continue;
		if (connect_signal_count.load() > 0) {
			connect_signal_count.fetch_sub(1);
		}
		else if (started_connect_count.load() != 0) {
			continue;
		}

		if (ConnectToServer()) {
			started_connect_count.fetch_add(1);
			last_connect_time.store(GetCurrentTimeMS());
		}
		else {
			connect_signal_count.fetch_add(1);
			std::this_thread::sleep_for(std::chrono::milliseconds(100));
		}
	}
}

void TestManager::ProcessConnect(int user_index, ExOverlapped* connect_over)
{
	Session* session = sessions[user_index];
	if (session->GetState() != CONNECTING) {
		delete connect_over;
		return;
	}
	setsockopt(session->GetSocket(), SOL_SOCKET, SO_UPDATE_CONNECT_CONTEXT, nullptr, 0);
	connected_client.fetch_add(1);
	{
		std::lock_guard<std::mutex> lock(log_mutex);
		std::cout << "[connect] client_index: " << user_index << std::endl;
	}
	session->SetState(LOGIN);
	session->RecvPacket(iocp_handle);
	SendTestLoginPacket(user_index);
	delete connect_over;
}

void TestManager::ProcessGQCS()
{
	DWORD transferred_bytes = 0;
	ULONG_PTR key = 0;
	WSAOVERLAPPED* over = nullptr;
	BOOL result = GetQueuedCompletionStatus(
		iocp_handle,
		&transferred_bytes,
		&key,
		&over,
		INFINITE);

	if (!over) return;
	int user_index = static_cast<int>(key);
	if (user_index < 0 || user_index >= MAX_USER) return;

	ExOverlapped* ex_over = reinterpret_cast<ExOverlapped*>(over);
	if (!result) {
		OP_TYPE op_type = ex_over->op_type;
		if (op_type == SEND || op_type == CONNECT) delete ex_over;
		Disconnect(user_index);
		return;
	}

	if (ex_over->op_type != CONNECT && transferred_bytes == 0) {
		if (ex_over->op_type == SEND) delete ex_over;
		Disconnect(user_index);
		return;
	}

	switch (ex_over->op_type) {
	case CONNECT:
		ProcessConnect(user_index, ex_over);
		break;

	case RECV:
		ProcessPacket(static_cast<int>(transferred_bytes), user_index);
		if (sessions[user_index]->GetState() != NONE && sessions[user_index]->GetState() != DISCONNECTING) {
			sessions[user_index]->RecvPacket(iocp_handle);
		}
		break;

	case SEND:
		delete ex_over;
		break;
	}
}

void TestManager::SendTestLoginPacket(int user_index)
{
	C2S_TEST_LOGIN_PACKET p{};
	p.header.size = static_cast<std::uint16_t>(sizeof(p));
	p.header.type = C2S_TEST_LOGIN;
	p.temp_id = user_index;
	p.client_time = static_cast<std::uint64_t>(GetCurrentTimeMS());
	sessions[user_index]->SendPacket(reinterpret_cast<char*>(&p), iocp_handle);
}

void TestManager::SendStressEnterMatchPacket(int user_index)
{
	C2S_STRESS_ENTER_MATCH_PACKET p{};
	p.header.size = static_cast<std::uint16_t>(sizeof(p));
	p.header.type = C2S_STRESS_ENTER_MATCH;
	sessions[user_index]->SendPacket(reinterpret_cast<char*>(&p), iocp_handle);
}

void TestManager::SendTestMovePacket(int user_index)
{
	Session* session = sessions[user_index];
	std::uint32_t sequence = session->move_sequence.fetch_add(1) + 1;
	C2S_TEST_MOVE_PACKET p{};
	p.header.size = static_cast<std::uint16_t>(sizeof(p));
	p.header.type = C2S_TEST_MOVE;
	p.move_type = static_cast<char>(sequence % 5);
	p.sequence = sequence;
	p.client_time = static_cast<std::uint64_t>(GetCurrentTimeMS());
	session->SendPacket(reinterpret_cast<char*>(&p), iocp_handle);
}

void TestManager::ProcessSend()
{
	long long now_time = GetCurrentTimeMS();
	for (int i = 0; i < STRESS_SESSION_COUNT; ++i) {
		Session* client = sessions[i];
		if (client->GetState() != PLAYING) continue;
		long long expected = client->last_send_time;
		if (now_time - expected < STRESS_MOVE_INTERVAL_MS) continue;
		if (client->last_send_time.compare_exchange_strong(expected, now_time)) {
			SendTestMovePacket(i);
		}
	}
	PrintMetrics(now_time);
}

int TestManager::GetClientIndex()
{
	for (int i = 0; i < MAX_USER; ++i) {
		if (sessions[i]->GetState() == NONE) {
			if (sessions[i]->SetState(NONE, CONNECTING)) {
				return i;
			}
		}
	}

	return -1;
}

void TestManager::SetTestMessege(int message_size)
{
	std::ifstream in("message.txt", std::ios::binary);
	if (!in.is_open()) return;
	in.read(test_message, message_size);
	in.close();
}

void TestManager::ProcessPacket(int recv_bytes, int user_index)
{
	Session* session = sessions[user_index];
	if (session->GetState() == NONE) return;

	if (recv_bytes + session->GetRemainDataSize() > BUF_SIZE) {
		Disconnect(user_index);
		return;
	}

	session->SetRemainDataSize(recv_bytes);
	int remain_data_size = session->GetRemainDataSize();
	int offset = 0;
	char p_buffer[BUF_SIZE];
	memcpy(p_buffer, session->GetExOver().packet_buf, remain_data_size);

	while (remain_data_size - offset >= static_cast<int>(sizeof(PacketHeader))) {
		PacketHeader* header = reinterpret_cast<PacketHeader*>(p_buffer + offset);
		std::uint16_t packet_size = header->size;
		if (packet_size < sizeof(PacketHeader) || packet_size > BUF_SIZE) {
			Disconnect(user_index);
			return;
		}
		if (remain_data_size - offset < packet_size) break;

		HandlePacket(p_buffer + offset, user_index);
		offset += packet_size;
		if (session->GetState() == NONE) return;
	}

	session->SetRemainDataSize(-offset);
	memmove(session->GetExOver().packet_buf, session->GetExOver().packet_buf + offset, session->GetRemainDataSize());
}

void TestManager::Disconnect(int session_index)
{
	if (session_index < 0 || session_index >= MAX_USER) return;
	Session* session = sessions[session_index];
	SESSION_STATE prev_state = session->GetState();
	while (prev_state != NONE && prev_state != DISCONNECTING) {
		if (session->SetState(prev_state, DISCONNECTING)) break;
		prev_state = session->GetState();
	}
	if (prev_state == NONE || prev_state == DISCONNECTING) return;

	if (prev_state != CONNECTING) {
		C2S_DISCONNECT_PACKET p{};
		p.header.size = static_cast<std::uint16_t>(sizeof(p));
		p.header.type = C2S_DISCONNECT;
		session->SendPacket(reinterpret_cast<char*>(&p), iocp_handle);
		connected_client.fetch_sub(1);
		{
			std::lock_guard<std::mutex> lock(log_mutex);
			std::cout << "[disconnect] client_index: " << session_index << std::endl;
		}
	}
	if (prev_state == PLAYING) playing_client.fetch_sub(1);
	ReduceStartedConnectCount();

	closesocket(session->GetSocket());
	session->ClearSession();
	session->SetSocket(INVALID_SOCKET);
	session->SetState(NONE);
	connect_cv.notify_one();
}

void TestManager::HandlePacket(char* packet, int user_index)
{
	PacketHeader* header = reinterpret_cast<PacketHeader*>(packet);
	Session* session = sessions[user_index];

	switch (header->type) {
	case S2C_TEST_LOGIN: {
		S2C_TEST_LOGIN_PACKET* p = reinterpret_cast<S2C_TEST_LOGIN_PACKET*>(packet);
		current_login_latency.store(GetCurrentTimeMS() - static_cast<long long>(p->client_time));
		long long total_latency = login_latency_total.fetch_add(current_login_latency.load()) + current_login_latency.load();
		long long login_count = login_latency_count.fetch_add(1) + 1;
		average_login_latency.store(total_latency / login_count);
		long long expected = max_login_latency.load();
		while (current_login_latency.load() > expected && !max_login_latency.compare_exchange_weak(expected, current_login_latency.load())) {
		}
		NotifyLoginSuccess();
		if (p->id >= 0 && session->GetState() == LOGIN) {
			session->SetId(p->id);
			session->SetState(LOBBY);
			SendStressEnterMatchPacket(user_index);
			session->SetState(ENTER_ROOM);
		}
		break;
	}

	case S2C_LOGIN: {
		S2C_LOGIN_PACKET* p = reinterpret_cast<S2C_LOGIN_PACKET*>(packet);
		UpdateLoginLatency(user_index);
		NotifyLoginSuccess();
		if (p->id >= 0 && session->GetState() == LOGIN) {
			session->SetId(p->id);
			session->SetState(LOBBY);
			SendStressEnterMatchPacket(user_index);
			session->SetState(ENTER_ROOM);
		}
		break;
	}

	case S2C_SINGLE_START:
	case S2C_MULTI_START: {
		if (session->GetState() != PLAYING) {
			session->last_send_time = GetCurrentTimeMS();
			session->SetState(PLAYING);
			playing_client.fetch_add(1);
		}
		break;
	}

	case S2C_TEST_MOVE: {
		S2C_TEST_MOVE_PACKET* p = reinterpret_cast<S2C_TEST_MOVE_PACKET*>(packet);
		if (p->id != session->GetId()) break;
		long long now_time = GetCurrentTimeMS();
		UpdateLatency(now_time - static_cast<long long>(p->client_time));
		break;
	}

	case S2C_TEST: {
		S2C_TEST_PACKET* p = reinterpret_cast<S2C_TEST_PACKET*>(packet);
		AdjustSessionNumber(GetCurrentTimeMS(), p);
		break;
	}

	case S2C_DISCONNECT:
		Disconnect(user_index);
		break;

	default:
		break;
	}
}

void TestManager::UpdateLatency(long long latency)
{
	current_latency.store(latency);
	long long average_latency = 0;
	{
		std::lock_guard<std::mutex> lock(latency_mutex);
		if (recent_latency_count < recent_latencies.size()) {
			++recent_latency_count;
		}
		else {
			recent_latency_total -= recent_latencies[recent_latency_index];
		}
		recent_latencies[recent_latency_index] = latency;
		recent_latency_total += latency;
		recent_latency_index = (recent_latency_index + 1) % static_cast<int>(recent_latencies.size());
		average_latency = recent_latency_count == 0 ? 0 : recent_latency_total / recent_latency_count;
	}
	recent_average_latency.store(average_latency);
	delay.store(GetConnectDelay(average_latency));
	if (average_latency > 100) {
		bool expected = true;
		if (connect_enabled.compare_exchange_strong(expected, false)) {
			connect_cv.notify_one();
		}
	}
	else if (average_latency > 100) {
		connect_cv.notify_one();
	}

	long long expected = max_latency.load();
	while (latency > expected && !max_latency.compare_exchange_weak(expected, latency)) {
	}
}

void TestManager::UpdateLoginLatency(int user_index)
{
	long long send_time = sessions[user_index]->login_send_time.exchange(-1);
	if (send_time < 0) return;

	long long latency = GetCurrentTimeMS() - send_time;
	current_login_latency.store(latency);
	long long total_latency = login_latency_total.fetch_add(latency) + latency;
	long long login_count = login_latency_count.fetch_add(1) + 1;
	average_login_latency.store(total_latency / login_count);

	long long expected = max_login_latency.load();
	while (latency > expected && !max_login_latency.compare_exchange_weak(expected, latency)) {
	}
}

void TestManager::PrintMetrics(long long now_time)
{
	long long expected = last_print_time.load();
	if (now_time - expected < 1000) return;
	if (!last_print_time.compare_exchange_strong(expected, now_time)) return;

	long long average_latency = recent_average_latency.load();
	std::cout << "connected: " << connected_client.load()
		<< " playing: " << playing_client.load()
		<< " current_rtt: " << current_latency.load() << "ms"
		<< " avg_rtt: " << average_latency << "ms"
		<< " max_rtt: " << max_latency.load() << "ms"
		<< " login_rtt: " << current_login_latency.load() << "ms"
		<< " avg_login: " << average_login_latency.load() << "ms"
		<< " connect: " << (connect_enabled.load() ? "on" : "off")
		<< std::endl;
	SendViewMetrics();
}

void TestManager::SendViewMetrics()
{
	SOCKET socket_copy = INVALID_SOCKET;
	{
		std::lock_guard<std::mutex> lock(view_mutex);
		socket_copy = view_socket;
	}
	if (socket_copy == INVALID_SOCKET) return;

	long long average_latency = recent_average_latency.load();
	S2V_STRESS_METRICS_PACKET packet{};
	packet.header.size = static_cast<std::uint16_t>(sizeof(packet));
	packet.header.type = S2V_STRESS_METRICS;
	packet.metrics.connected_client = static_cast<std::uint64_t>(connected_client.load());
	packet.metrics.playing_client = static_cast<std::uint64_t>(playing_client.load());
	packet.metrics.current_latency_ms = static_cast<std::uint64_t>(current_latency.load());
	packet.metrics.average_latency_ms = static_cast<std::uint64_t>(average_latency);
	packet.metrics.max_latency_ms = static_cast<std::uint64_t>(max_latency.load());
	packet.metrics.current_login_latency_ms = static_cast<std::uint64_t>(current_login_latency.load());
	packet.metrics.average_login_latency_ms = static_cast<std::uint64_t>(average_login_latency.load());
	packet.metrics.max_login_latency_ms = static_cast<std::uint64_t>(max_login_latency.load());
	packet.metrics.connect_enabled = connect_enabled.load() ? 1 : 0;

	int result = send(socket_copy, reinterpret_cast<const char*>(&packet), sizeof(packet), 0);
	if (result == SOCKET_ERROR) {
		std::lock_guard<std::mutex> lock(view_mutex);
		if (view_socket == socket_copy) {
			closesocket(view_socket);
			view_socket = INVALID_SOCKET;
		}
	}
}

void TestManager::SetConnectEnabled(bool enabled)
{
	connect_enabled.store(enabled);
	if (enabled) {
		connect_signal_count.fetch_add(1);
	}
	connect_cv.notify_one();
	SendViewMetrics();
}
