#pragma once
#include <WinSock2.h>
#include <MSWSock.h>
#include <array>
#include <atomic>
#include <memory>
#include "Types.h"
#include "ExOverlapped.h"
#include "ActiveRoomManager.h"
#include "ActiveUserManager.h"
#include "Session.h"
#include "PacketHandler.h"
#include "DBResultHandler.h"
#include "MQueue.h"
#include "TetrisRoom.h"
#include "Atomic.h"
#include "Database.h"
#include "RankingManager.h"
#pragma comment(lib, "MSWSock.lib")
#pragma comment(lib, "Ws2_32.lib")

class IOCPServer
{
	HANDLE iocp_handle;
	SOCKET listen_socket, client_socket;
	WSADATA wsadata;
	SOCKADDR_IN server_addr;
	IOOverlapped accept_over;
	Database db;
	RankingManager ranking_manager;
	ActiveRoomManager active_rooms;
	ActiveUserManager active_users;
	PacketHandler packet_handler;
	DBResultHandler db_result_handler;
	std::atomic<int> room_gen_generator = -1;
	std::atomic<long long> tick_count = 0;
	std::array<std::atomic<SP<Session>>, MAX_USER> users;
	
	std::array<std::atomic<SP<TetrisRoom>>, MAX_ROOM> rooms;

	std::atomic<bool> is_running = true;

	friend class PacketHandler;
	friend class DBResultHandler;

public:
	IOCPServer();
	~IOCPServer();

	int GetEmptyUserIndex();
	int GetEmptyRoomIndex();
	int GetNewRoomGen();
	bool GetRunning() const { return is_running.load(); }
	HANDLE GetHandle() const { return iocp_handle; }

	long long GetTickCount() const { return tick_count.load(); }
	SP<TetrisRoom> GetRoom(int room_index) const;
	Database& GetDB() { return db; }
	RankingManager& GetRankingManager() { return ranking_manager; }

	void AddTickCount() { tick_count.fetch_add(1); }

	SP<Session> FindSessionByIndex(int user_index);

	void BeginDisconnect(const SP<Session>& session);
	void TryDisconnect(const SP<Session>& session);
	void Disconnect(const SP<Session>& session);
	void StartServer();
	void ProcessGQCS();
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
	int TryJoinRoom(const SP<Session>& session, int room_gen, const std::string& room_password);
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
