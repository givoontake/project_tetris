#pragma once
#include <WinSock2.h>
#include <MSWSock.h>
#include <array>
#include <atomic>
#include <memory>
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
	std::array<Session, MAX_USER> users;

	std::array<std::atomic<std::shared_ptr<TetrisRoom>>, MAX_ROOM> rooms;

	bool is_running = true;

	friend class PacketHandler;
	friend class DBResultHandler;

public:
	IOCPServer();
	~IOCPServer();

	int GetEmptyUserIndex();
	int GetEmptyRoomIndex();
	int GetNewRoomGen();
	bool GetRunning() const { return is_running; }
	HANDLE GetHandle() const { return iocp_handle; }

	long long GetTickCount() const { return tick_count.load(); }
	std::shared_ptr<TetrisRoom> GetRoom(int room_index) const { return std::atomic_load(&rooms[room_index]); }
	Database& GetDB() { return db; }
	RankingManager& GetRankingManager() { return ranking_manager; }

	void AddTickCount() { tick_count.fetch_add(1); }

	Session& FindSessionByIndex(int user_index) { return users[user_index]; }
	int FindSessionIndexById(int user_id);

	void TryDisconnect(Session& session);
	void Disconnect(Session& session);
	void StartServer();
	void ProcessGQCS();
	bool ProcessPacket(Session& session, int recv_bytes);
	void RoutePacket(char* packet, Session& session);
	void BroadCastToLobby(char* packet);
	void CreateOpenRoom(char* packet, Session& session);
	void CreateLockRoom(char* packet, Session& session);
	void DeleteRoom(int room_index);
	void RequestLoadRanking();
	void StringToCharBuf(const std::string& str, char* buf, int buf_size);
	std::string CharBufToString(const char* buf, int buf_size);
	void SendRoomList(Session& session);
	bool TryJoinRoom(Session& session, int room_gen, const std::string& room_password);
	int FindRoom(int room_gen);
	int FindUser(int user_id);
	void SendError(Session& session, int error_code);
	bool CheckDuplicateId(const int user_id);
	void FindMatch(Session& session, int max_user);
	void SendLobbyUserList(Session& session);
	void SendFriendList(Session& session);
	void SendRanking(Session& session);
	void SendAddFriendResult(FriendInfo& requester_info, FriendInfo& recver_info);
	void SendDeleteFriendResult(int requester_id, int target_id);
};
