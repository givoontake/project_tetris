#pragma once
#include <WinSock2.h>
#include <MSWSock.h>
#include <array>
#include <atomic>
#include <memory>
#include "types.h"
#include "ExOverlapped.h"
#include "ActivePlayerManager.h"
#include "Session.h"
#include "DBResultHandler.h"
#include "DBTasks.h"
#include "TetrisRoom.h"
#include "RankingManager.h"
#pragma comment(lib, "MSWSock.lib")
#pragma comment(lib, "Ws2_32.lib")

class ServerThreadManager;
struct LobbyTask;
struct RoomLifecycleTask;
struct SessionTask;

class TetrisServer
{
	HANDLE iocp_handle_;
	SOCKET listen_socket_, accept_socket_;
	WSADATA wsa_data_;
	SOCKADDR_IN server_addr_;
	IOOverlapped accept_over_;
	RankingManager ranking_manager_;
	ActivePlayerManager active_players_;
	DBResultHandler db_result_handler_;
	std::array<Session, MAX_PLAYER_COUNT> sessions_;
	std::array<std::atomic<SP<TetrisRoom>>, MAX_ROOM_COUNT> rooms_;

	std::atomic<bool> is_running_ = true;
	std::unique_ptr<ServerThreadManager> thread_manager_;

public:
	TetrisServer();
	~TetrisServer();

	Session* AcquireSession(SOCKET new_socket);
	std::uint64_t GenerateSessionID();
	bool IsRunning() const { return is_running_.load(); }
	void RequestStop() { is_running_.store(false); }
	HANDLE GetIOCPHandle() const { return iocp_handle_; }
	ServerThreadManager& GetThreadManager() { return *thread_manager_; }

	ActivePlayerManager& GetActivePlayerManager() { return active_players_; }
	RankingManager& GetRankingManager() { return ranking_manager_; }
	DBResultHandler& GetDBResultHandler() { return db_result_handler_; }

	Session* FindSessionByIndex(int session_index);
	Session* FindSession(SessionKey session_key);
	SP<TetrisRoom> GetRoomByIndex(int room_index) const;
	bool TryAddRoom(int room_index, const SP<TetrisRoom>& room);
	bool TryRemoveRoom(int room_index, const SP<TetrisRoom>& room);

	void RequestDisconnect(SessionKey session_key);
	void CompleteSessionIO(SessionKey session_key);
	void CompleteRoomDisconnect(SessionKey session_key);
	bool FinalizeDisconnect(SessionKey session_key);
	void StartServer();
	void CompleteAccept();
	void StartThreads();
	void CloseThreads();
	void JoinThreads();
	bool EnqueueDBTask(std::unique_ptr<ServerDBTask> db_task);
	bool EnqueueDBTask(std::unique_ptr<SessionDBTask> db_task);
	void EnqueueDBTask(std::unique_ptr<MultiSessionDBTask> db_task);
	bool ProcessRecvBuffer(Session& session, SessionKey session_key, int recv_bytes);
	void RoutePacket(char* packet, Session& session);
	SessionTaskProcessResult ProcessSessionTask(Session& session, std::unique_ptr<SessionTask> task);
	bool EnqueueSessionTask(SessionKey session_key, std::unique_ptr<SessionTask> task);
	void EnqueueLobbyTask(std::unique_ptr<LobbyTask> task);
	void EnqueueRoomLifecycleTask(std::unique_ptr<RoomLifecycleTask> task);
	void RequestLoadRankings();
	void StringToCharBuf(const std::string& str, char* buf, int buf_size);
	std::string CharBufToString(const char* buf, int buf_size);
	void SendError(Session& session, int error_code);
};
