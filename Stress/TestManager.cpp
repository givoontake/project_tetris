#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <thread>
#include <utility>
#include <WS2tcpip.h>
#include "TestManager.h"

TestManager::TestManager(const std::string& server_ip, std::string output_directory)
	: latency_recorder_(std::move(output_directory))
{
	for (auto& session : sessions_) session = std::make_unique<Session>();
	for (auto& room_key : room_keys_) room_key.store(0);
	for (auto& room_join_ready_time : room_join_ready_times_) room_join_ready_time.store(-1);

	WSAStartup(MAKEWORD(2, 2), &wsa_data_);
	server_address_.sin_family = AF_INET;
	server_address_.sin_port = htons(PORT_NUM);
	if (inet_pton(AF_INET, server_ip.c_str(), &server_address_.sin_addr) != 1) throw std::invalid_argument("invalid server IPv4 address: " + server_ip);
	iocp_handle_ = CreateIoCompletionPort(INVALID_HANDLE_VALUE, nullptr, 0, 0);
}

TestManager::~TestManager()
{
	for (auto& session : sessions_) {
		if (session->GetSocket() != INVALID_SOCKET) closesocket(session->GetSocket());
	}
	if (iocp_handle_) CloseHandle(iocp_handle_);
	WSACleanup();
}

void TestManager::Start(int client_count)
{
	target_client_count_.store((std::min)(client_count, MAX_USER));
	started_connect_count_.store(0);
	ready_room_count_.store(0);
	is_reconnect_enabled_.store(true);
	connect_signal_count_.store((std::min)(CONNECT_INFLIGHT_COUNT, target_client_count_.load()));
	connect_cv_.notify_all();
}

long long TestManager::GetCurrentTimeMS() const
{
	return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
}

bool TestManager::LoadConnectEx(SOCKET socket)
{
	std::lock_guard<std::mutex> lock(connect_ex_mutex_);
	if (connect_ex_) return true;
	GUID guid = WSAID_CONNECTEX;
	DWORD bytes = 0;
	return WSAIoctl(socket, SIO_GET_EXTENSION_FUNCTION_POINTER, &guid, sizeof(guid), &connect_ex_, sizeof(connect_ex_), &bytes, nullptr, nullptr) == 0;
}

int TestManager::FindAvailableSessionIndex()
{
	const int target_count = target_client_count_.load();
	for (int i = 0; i < target_count; ++i) {
		if (sessions_[i]->TrySetState(SessionState::NONE, SessionState::CONNECTING)) return i;
	}
	return -1;
}

bool TestManager::ConnectToServer()
{
	const int session_index = FindAvailableSessionIndex();
	if (session_index < 0) return false;
	Session& session = *sessions_[session_index];

	SOCKET socket = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, nullptr, 0, WSA_FLAG_OVERLAPPED);
	if (socket == INVALID_SOCKET || !LoadConnectEx(socket)) {
		if (socket != INVALID_SOCKET) closesocket(socket);
		session.SetState(SessionState::NONE);
		return false;
	}

	SOCKADDR_IN local_address{};
	local_address.sin_family = AF_INET;
	local_address.sin_addr.s_addr = htonl(INADDR_ANY);
	if (bind(socket, reinterpret_cast<SOCKADDR*>(&local_address), sizeof(local_address)) == SOCKET_ERROR) {
		closesocket(socket);
		session.SetState(SessionState::NONE);
		return false;
	}

	session.InitSession();
	session.SetIndex(session_index);
	session.SetSocket(socket);
	CreateIoCompletionPort(reinterpret_cast<HANDLE>(socket), iocp_handle_, session_index, 0);

	ExOverlapped* connect_over = new ExOverlapped;
	connect_over->SetOperationType(OperationType::CONNECT);
	const BOOL result = connect_ex_(socket, reinterpret_cast<SOCKADDR*>(&server_address_), sizeof(server_address_), nullptr, 0, nullptr, &connect_over->over);
	if (!result && WSAGetLastError() != WSA_IO_PENDING) {
		delete connect_over;
		closesocket(socket);
		session.SetSocket(INVALID_SOCKET);
		session.SetState(SessionState::NONE);
		return false;
	}
	return true;
}

void TestManager::ProcessConnect()
{
	while (true) {
		{
			std::unique_lock<std::mutex> lock(connect_mutex_);
			connect_cv_.wait(lock, [this] { return connect_signal_count_.load() > 0 || !is_reconnect_enabled_.load(); });
		}
		if (!is_reconnect_enabled_.load()) return;

		if (started_connect_count_.load() >= target_client_count_.load()) {
			std::this_thread::yield();
			continue;
		}
		if (connect_signal_count_.fetch_sub(1) <= 0) {
			connect_signal_count_.fetch_add(1);
			continue;
		}

		if (!is_reconnect_enabled_.load()) return;
		if (ConnectToServer()) {
			started_connect_count_.fetch_add(1);
		}
		else {
			if (is_reconnect_enabled_.load()) connect_signal_count_.fetch_add(1);
			std::this_thread::sleep_for(std::chrono::milliseconds(10));
		}
	}
}

void TestManager::CompleteConnect(int session_index, ExOverlapped* connect_over)
{
	Session& session = *sessions_[session_index];
	setsockopt(session.GetSocket(), SOL_SOCKET, SO_UPDATE_CONNECT_CONTEXT, nullptr, 0);
	session.SetState(SessionState::LOGIN);
	session.RecvPacket(iocp_handle_);
	const int connected_count = connected_client_count_.fetch_add(1) + 1;
	if (connected_count <= 5 || connected_count % 1000 == 0) {
		std::lock_guard<std::mutex> lock(log_mutex_);
		std::cout << "connected=" << connected_count << std::endl;
	}
	SendTestLogin(session_index);
	delete connect_over;
}

void TestManager::ProcessIO()
{
	DWORD transferred_bytes = 0;
	ULONG_PTR key = 0;
	WSAOVERLAPPED* over = nullptr;
	const BOOL result = GetQueuedCompletionStatus(iocp_handle_, &transferred_bytes, &key, &over, INFINITE);
	if (!over) return;

	const int session_index = static_cast<int>(key);
	if (session_index < 0 || session_index >= target_client_count_.load()) return;
	ExOverlapped* ex_over = reinterpret_cast<ExOverlapped*>(over);
	if (!result || (ex_over->operation_type != OperationType::CONNECT && transferred_bytes == 0)) {
		if (ex_over->operation_type == OperationType::SEND || ex_over->operation_type == OperationType::CONNECT) delete ex_over;
		Disconnect(session_index);
		return;
	}

	switch (ex_over->operation_type) {
	case OperationType::CONNECT:
		CompleteConnect(session_index, ex_over);
		break;
	case OperationType::RECV:
		ProcessRecvBuffer(static_cast<int>(transferred_bytes), session_index);
		if (sessions_[session_index]->GetState() != SessionState::NONE && sessions_[session_index]->GetState() != SessionState::DISCONNECTING) {
			sessions_[session_index]->RecvPacket(iocp_handle_);
		}
		break;
	case OperationType::SEND:
		delete ex_over;
		break;
	}
}

void TestManager::ProcessRecvBuffer(int recv_bytes, int session_index)
{
	Session& session = *sessions_[session_index];
	if (recv_bytes + session.GetRemainingDataSize() > BUF_SIZE) {
		Disconnect(session_index);
		return;
	}

	session.AdjustRemainingDataSize(recv_bytes);
	int remaining_data_size = session.GetRemainingDataSize();
	int offset = 0;
	char packet_buffer[BUF_SIZE];
	memcpy(packet_buffer, session.GetRecvOverlapped().packet_buffer, remaining_data_size);

	while (remaining_data_size - offset >= static_cast<int>(sizeof(PacketHeader))) {
		PacketHeader* header = reinterpret_cast<PacketHeader*>(packet_buffer + offset);
		if (header->size < sizeof(PacketHeader) || header->size > BUF_SIZE) {
			Disconnect(session_index);
			return;
		}
		if (remaining_data_size - offset < header->size) break;
		HandlePacket(packet_buffer + offset, session_index);
		offset += header->size;
		if (session.GetState() == SessionState::NONE || session.GetState() == SessionState::DISCONNECTING) return;
	}

	session.AdjustRemainingDataSize(-offset);
	memmove(session.GetRecvOverlapped().packet_buffer, session.GetRecvOverlapped().packet_buffer + offset, session.GetRemainingDataSize());
}

void TestManager::SendTestLogin(int session_index)
{
	C2S_TEST_LOGIN_PACKET packet{};
	packet.header.size = static_cast<std::uint16_t>(sizeof(packet));
	packet.header.type = C2S_TEST_LOGIN;
	packet.player_id = session_index + 1;
	std::snprintf(packet.login_id, sizeof(packet.login_id), "test%d", packet.player_id);
	std::snprintf(packet.login_password, sizeof(packet.login_password), "1234");
	sessions_[session_index]->SendPacket(reinterpret_cast<char*>(&packet), iocp_handle_);
}

void TestManager::SendCreateRoom(int host_index)
{
	Session& session = *sessions_[host_index];
	if (!session.TrySetState(SessionState::LOBBY, SessionState::ENTER_ROOM)) return;
	C2S_ADD_PUBLIC_ROOM_PACKET packet{};
	packet.header.size = static_cast<std::uint16_t>(sizeof(packet));
	packet.header.type = C2S_ADD_PUBLIC_ROOM;
	packet.max_player_count = ROOM_PLAYER_COUNT;
	std::snprintf(packet.room_name, sizeof(packet.room_name), "stress_%d", host_index / ROOM_PLAYER_COUNT);
	session.SendPacket(reinterpret_cast<char*>(&packet), iocp_handle_);
}

void TestManager::TryJoinRoom(int session_index)
{
	if (session_index % ROOM_PLAYER_COUNT == 0) return;
	Session& session = *sessions_[session_index];
	const int group_index = session_index / ROOM_PLAYER_COUNT;
	const RoomKey room_key = room_keys_[group_index].load();
	if (room_key == 0 || GetCurrentTimeMS() < room_join_ready_times_[group_index].load()) return;
	if (!session.TrySetState(SessionState::LOBBY, SessionState::ENTER_ROOM)) return;

	C2S_JOIN_PUBLIC_ROOM_PACKET packet{};
	packet.header.size = static_cast<std::uint16_t>(sizeof(packet));
	packet.header.type = C2S_JOIN_PUBLIC_ROOM;
	packet.room_key = room_key;
	session.SendPacket(reinterpret_cast<char*>(&packet), iocp_handle_);
}

void TestManager::SendReady(int session_index)
{
	C2S_READY_PACKET packet{};
	packet.header.size = static_cast<std::uint16_t>(sizeof(packet));
	packet.header.type = C2S_READY;
	sessions_[session_index]->SendPacket(reinterpret_cast<char*>(&packet), iocp_handle_);
}

void TestManager::SendStart(int host_index)
{
	C2S_START_PACKET packet{};
	packet.header.size = static_cast<std::uint16_t>(sizeof(packet));
	packet.header.type = C2S_START;
	sessions_[host_index]->SendPacket(reinterpret_cast<char*>(&packet), iocp_handle_);
}

void TestManager::SendMove(int session_index, long long send_time)
{
	Session& session = *sessions_[session_index];
	if (!session.PushMoveSendTime(send_time)) return;
	C2S_MOVE_PACKET packet{};
	packet.header.size = static_cast<std::uint16_t>(sizeof(packet));
	packet.header.type = C2S_MOVE;
	packet.move_type = static_cast<char>(session.GetNextMoveType());
	session.SendPacket(reinterpret_cast<char*>(&packet), iocp_handle_);
}

void TestManager::NotifyLoginComplete()
{
	if (is_reconnect_enabled_.load() && started_connect_count_.load() < target_client_count_.load()) connect_signal_count_.fetch_add(1);
	connect_cv_.notify_one();
}

void TestManager::HandlePacket(char* packet, int session_index)
{
	PacketHeader* header = reinterpret_cast<PacketHeader*>(packet);
	Session& session = *sessions_[session_index];

	switch (header->type) {
	case S2C_TEST_LOGIN: {
		S2C_TEST_LOGIN_PACKET* login_packet = reinterpret_cast<S2C_TEST_LOGIN_PACKET*>(packet);
		NotifyLoginComplete();
		if (login_packet->player_id < 0 || !session.TrySetState(SessionState::LOGIN, SessionState::LOBBY)) break;
		session.SetPlayerID(login_packet->player_id);
		if (session_index % ROOM_PLAYER_COUNT == 0) SendCreateRoom(session_index);
		else TryJoinRoom(session_index);
		break;
	}
	case S2C_ADD_PUBLIC_ROOM: {
		S2C_ADD_PUBLIC_ROOM_PACKET* room_packet = reinterpret_cast<S2C_ADD_PUBLIC_ROOM_PACKET*>(packet);
		session.SetState(SessionState::ROOM);
		const int group_index = session_index / ROOM_PLAYER_COUNT;
		if (session_index % ROOM_PLAYER_COUNT == 0) {
			room_keys_[group_index].store(room_packet->room_key);
			room_join_ready_times_[group_index].store(GetCurrentTimeMS() + 100);
		}
		else {
			SendReady(session_index);
		}
		break;
	}
	case S2C_READY: {
		if (session_index % ROOM_PLAYER_COUNT != 0) break;
		S2C_READY_PACKET* ready_packet = reinterpret_cast<S2C_READY_PACKET*>(packet);
		if (!ready_packet->is_ready || session.IncrementReadyPlayerCount() != ROOM_PLAYER_COUNT - 1) break;
		session.ResetReadyPlayerCount();
		const int target_room_count = target_client_count_.load() / ROOM_PLAYER_COUNT;
		if (ready_room_count_.load() == target_room_count) SendStart(session_index);
		else if (ready_room_count_.fetch_add(1) + 1 == target_room_count) {
			for (int host_index = 0; host_index < target_client_count_.load(); host_index += ROOM_PLAYER_COUNT) SendStart(host_index);
		}
		break;
	}
	case S2C_MULTI_START: {
		if (session.GetState() == SessionState::PLAYING) break;
		session.SetLastSendTime(GetCurrentTimeMS());
		session.SetState(SessionState::PLAYING);
		const int playing_count = playing_client_count_.fetch_add(1) + 1;
		if (playing_count <= 5 || playing_count % 1000 == 0) {
			std::lock_guard<std::mutex> lock(log_mutex_);
			std::cout << "playing=" << playing_count << std::endl;
		}
		UpdateMeasurement();
		break;
	}
	case S2C_GAME_END:
		if (session.GetState() != SessionState::PLAYING) break;
		session.SetState(SessionState::ROOM);
		playing_client_count_.fetch_sub(1);
		if (session_index % ROOM_PLAYER_COUNT != 0) SendReady(session_index);
		break;
	case S2C_MOVE: {
		S2C_MOVE_PACKET* move_packet = reinterpret_cast<S2C_MOVE_PACKET*>(packet);
		if (move_packet->player_id != session.GetPlayerID()) break;
		const std::optional<long long> send_time = session.PopMoveSendTime();
		if (send_time) latency_recorder_.Record(GetCurrentTimeMS() - *send_time);
		break;
	}
	case S2C_ERROR: {
		S2C_ERROR_PACKET* error_packet = reinterpret_cast<S2C_ERROR_PACKET*>(packet);
		if (session.GetState() == SessionState::ENTER_ROOM) session.SetState(SessionState::LOBBY);
		latency_recorder_.RecordServerError(error_packet->error_code);
		std::lock_guard<std::mutex> lock(log_mutex_);
		std::cerr << "server_error session=" << session_index << " code=" << error_packet->error_code << std::endl;
		break;
	}
	case S2C_DISCONNECT:
		Disconnect(session_index);
		break;
	default:
		break;
	}
}

void TestManager::ProcessSend(int worker_index)
{
	const long long now_time = GetCurrentTimeMS();
	const int target_count = target_client_count_.load();
	for (int session_index = worker_index; session_index < target_count; session_index += SEND_THREAD_COUNT) {
		Session& session = *sessions_[session_index];
		if (session.GetState() == SessionState::LOBBY) {
			TryJoinRoom(session_index);
			continue;
		}
		if (session.GetState() != SessionState::PLAYING) continue;
		long long expected = session.GetLastSendTime();
		if (now_time - expected < MOVE_INTERVAL_MS) continue;
		if (session.TrySetLastSendTime(expected, now_time)) SendMove(session_index, now_time);
	}
	if (worker_index == 0) UpdateMeasurement();
}

void TestManager::UpdateMeasurement()
{
	if (playing_client_count_.load() == target_client_count_.load() && latency_recorder_.TryStart()) {
		is_reconnect_enabled_.store(false);
		connect_cv_.notify_all();
		std::lock_guard<std::mutex> lock(log_mutex_);
		std::cout << "measurement_started duration_seconds=" << MEASUREMENT_SECONDS << std::endl;
	}
	if (latency_recorder_.TryFinish()) {
		std::lock_guard<std::mutex> lock(log_mutex_);
		std::cout << "measurement_finished" << std::endl;
	}
}

void TestManager::Disconnect(int session_index)
{
	if (session_index < 0 || session_index >= target_client_count_.load()) return;
	Session& session = *sessions_[session_index];
	SessionState previous_state = session.GetState();
	while (previous_state != SessionState::NONE && previous_state != SessionState::DISCONNECTING) {
		if (session.TrySetState(previous_state, SessionState::DISCONNECTING)) break;
		previous_state = session.GetState();
	}
	if (previous_state == SessionState::NONE || previous_state == SessionState::DISCONNECTING) return;

	if (previous_state != SessionState::CONNECTING) connected_client_count_.fetch_sub(1);
	if (previous_state == SessionState::PLAYING) playing_client_count_.fetch_sub(1);
	closesocket(session.GetSocket());
	session.SetSocket(INVALID_SOCKET);
	session.ClearSession();
	session.SetState(SessionState::NONE);
	started_connect_count_.fetch_sub(1);
	if (!is_reconnect_enabled_.load()) return;
	connect_signal_count_.fetch_add(1);
	connect_cv_.notify_one();
}
