#pragma once
#include <WinSock2.h>
#include <MSWSock.h>
#include <array>
#include <atomic>
#include <memory>
#include "types.h"
#include "ExOverlapped.h"
#include "ActiveRoomManager.h"
#include "ActiveUserManager.h"
#include "Session.h"
#include "PacketHandler.h"
#include "DBResultHandler.h"
#include "TetrisRoom.h"
#include "LoginDBWorker.h"
#include "GameDBWorker.h"
#include "RankingManager.h"
#include "Threads/ServerThreadManager.h"
#pragma comment(lib, "MSWSock.lib")
#pragma comment(lib, "Ws2_32.lib")

static_assert(ServerThreadManager::GAME_DB_WORKER_COUNT > 0);
static_assert(ServerThreadManager::LOGIN_DB_WORKER_COUNT == 1);

class IOCPServer
{
	HANDLE iocp_handle_;
	SOCKET listen_socket_, client_socket_;
	WSADATA wsa_data_;
	SOCKADDR_IN server_addr_;
	IOOverlapped accept_over_;
	LoginDBWorker login_db_worker_;
	std::array<GameDBWorker, ServerThreadManager::GAME_DB_WORKER_COUNT> game_db_workers_;
	std::atomic<std::size_t> next_game_db_worker_{ 0 };
	RankingManager ranking_manager_;
	ActiveRoomManager active_rooms_;
	ActiveUserManager active_users_;
	PacketHandler packet_handler_;
	DBResultHandler db_result_handler_;
	std::atomic<int> room_gen_generator_ = -1;
	std::atomic<long long> tick_count_ = 0;
	std::array<std::atomic<SP<Session>>, MAX_USER> users_;
	std::array<std::atomic<SP<TetrisRoom>>, MAX_ROOM> rooms_;

	std::atomic<bool> is_running_ = true;

	friend class PacketHandler;
	friend class DBResultHandler;
	friend class IOThread;
	friend class ServerThreadManager;

public:
	IOCPServer();
	~IOCPServer();

	int GetEmptyUserIndex();
	int GetEmptyRoomIndex();
	int GetNewRoomGen();
	bool GetRunning() const { return is_running_.load(); }
	HANDLE GetHandle() const { return iocp_handle_; }

	long long GetTickCount() const { return tick_count_.load(); }
	SP<TetrisRoom> GetRoom(int room_index) const;
	RankingManager& GetRankingManager() { return ranking_manager_; }

	void AddTickCount() { tick_count_.fetch_add(1); }

	SP<Session> FindSessionByIndex(int user_index);

	void BeginDisconnect(const SP<Session>& session);
	void TryDisconnect(const SP<Session>& session);
	void Disconnect(const SP<Session>& session);
	void StartServer();
	void InitDBWorkers();
	void StartDBWorkers();
	void StopDBWorkers();
	void WakeDBWorkers();
	bool EnqueueDBTask(std::unique_ptr<ServerDBTask> db_task);
	bool EnqueueDBTask(std::unique_ptr<SessionDBTask> db_task, const SP<Session>& session);
	void EnqueueDBTask(std::unique_ptr<MultiSessionDBTask> db_task, const SP<Session> (&sessions)[MAX_MATCH_RESULT_PLAYERS]);
	void ProcessPacket(const SP<Session>& session, int recv_bytes);
	void RoutePacket(char* packet, const SP<Session>& session);
	void BroadCastToLobby(char* packet);
	void CreateOpenRoom(char* packet, const SP<Session>& session);
	void CreateLockRoom(char* packet, const SP<Session>& session);
	void DeleteRoom(int room_index);
	void RequestLoadRanking();
	void StringToCharBuf(const std::string& str, char* buf, int buf_size);
	std::string CharBufToString(const char* buf, int buf_size);
	void SendRoomList(const SP<Session>& session);
	int TryJoinRoom(const SP<Session>& session, int room_gen, const std::string& room_password, int matching_max_user = -1);
	SP<TetrisRoom> FindRoom(int room_gen);
	int FindUser(int user_id);
	void SendError(const SP<Session>& session, int error_code);
	void FindMatch(const SP<Session>& session, int max_user);
	void SendLobbyUserList(const SP<Session>& session);
	void SendFriendList(const SP<Session>& session);
	void SendRanking(const SP<Session>& session);
	void SendAddFriendResult(FriendInfo& requester_info, FriendInfo& recver_info);
	void SendDeleteFriendResult(int requester_id, int target_id);
};
