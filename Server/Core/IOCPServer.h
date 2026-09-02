#pragma once
#include <WinSock2.h>
#include <MSWSock.h>
#include <array>
#include <atomic>
#include <memory>
#include "types.h"
#include "ExOverlapped.h"
#include "ActiveRoomManager.h"
#include "ActivePlayerManager.h"
#include "Session.h"
#include "PacketHandler.h"
#include "DBResultHandler.h"
#include "TetrisRoom.h"
#include "LoginDBThread.h"
#include "GameDBThread.h"
#include "RankingManager.h"
#include "Threads/ServerThreadManager.h"
#pragma comment(lib, "MSWSock.lib")
#pragma comment(lib, "Ws2_32.lib")

static_assert(ServerThreadManager::GAME_DB_THREAD_COUNT > 0);
static_assert(ServerThreadManager::LOGIN_DB_THREAD_COUNT == 1);

class IOCPServer
{
	HANDLE iocp_handle_;
	SOCKET listen_socket_, accept_socket_;
	WSADATA wsa_data_;
	SOCKADDR_IN server_addr_;
	IOOverlapped accept_over_;
	LoginDBThread login_db_thread_;
	std::array<GameDBThread, ServerThreadManager::GAME_DB_THREAD_COUNT> game_db_threads_;
	std::atomic<std::size_t> next_game_db_thread_{ 0 };
	RankingManager ranking_manager_;
	ActiveRoomManager active_rooms_;
	ActivePlayerManager active_players_;
	PacketHandler packet_handler_;
	DBResultHandler db_result_handler_;
	std::atomic<int> room_gen_generator_ = -1;
	std::array<std::atomic<SP<Session>>, MAX_PLAYER_COUNT> sessions_;
	std::array<std::atomic<SP<TetrisRoom>>, MAX_ROOM_COUNT> rooms_;

	std::atomic<bool> is_running_ = true;

	friend class PacketHandler;
	friend class DBResultHandler;
	friend class IOThread;
	friend class ServerThreadManager;

public:
	IOCPServer();
	~IOCPServer();

	int FindAvailableSessionIndex();
	int GenerateRoomGen();
	bool IsRunning() const { return is_running_.load(); }
	HANDLE GetIOCPHandle() const { return iocp_handle_; }

	SP<TetrisRoom> GetRoomByIndex(int room_index) const;
	RankingManager& GetRankingManager() { return ranking_manager_; }

	SP<Session> FindSessionByIndex(int session_index);

	void BeginDisconnect(const SP<Session>& session);
	void TryDisconnect(const SP<Session>& session);
	void Disconnect(const SP<Session>& session);
	void StartServer();
	void InitDBThreads();
	void StartDBThreads();
	void StopDBThreads();
	void WakeDBThreads();
	bool EnqueueDBTask(std::unique_ptr<ServerDBTask> db_task);
	bool EnqueueDBTask(std::unique_ptr<SessionDBTask> db_task, const SP<Session>& session);
	void EnqueueDBTask(std::unique_ptr<MultiSessionDBTask> db_task, const SP<Session> (&sessions)[MAX_MATCH_RESULT_PLAYERS]);
	void ProcessRecvBuffer(const SP<Session>& session, int recv_bytes);
	void RoutePacket(char* packet, const SP<Session>& session);
	void BroadcastToLobby(char* packet);
	void CreatePublicRoom(char* packet, const SP<Session>& session);
	void CreatePrivateRoom(char* packet, const SP<Session>& session);
	void DeleteRoom(int room_index);
	void RequestLoadRankings();
	void StringToCharBuf(const std::string& str, char* buf, int buf_size);
	std::string CharBufToString(const char* buf, int buf_size);
	void SendRoomList(const SP<Session>& session);
	int TryJoinRoom(const SP<Session>& session, int room_gen, const std::string& room_password, int matching_max_player_count = -1);
	SP<TetrisRoom> FindRoomByGen(int room_gen);
	void SendError(const SP<Session>& session, int error_code);
	void FindMatch(const SP<Session>& session, int max_player_count);
	void SendLobbyPlayerList(const SP<Session>& session);
	void SendFriendList(const SP<Session>& session);
	void SendRankings(const SP<Session>& session);
	void SendAddFriendResult(FriendInfo& requester_info, FriendInfo& acceptor_info);
	void SendDeleteFriendResult(int requester_id, int target_id);
};
