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
	std::atomic<int> user_gen_generator = -1;
	std::atomic<int> room_gen_generator = -1;
	std::atomic<long long> tick_count = 0;
	std::array<Session, MAX_USER> users;

	std::array<std::atomic<std::shared_ptr<TetrisRoom>>, MAX_ROOM> rooms;

	bool is_running = true;

public:
	IOCPServer();
	~IOCPServer();

	int GetEmptyUserIndex();
	int GetEmptyRoomIndex();
	int GetNewUserGen();
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

	void Disconnect(int user_index);
	void StartServer();
	void ProcessGQCS();
	void ProcessPacket(Session& session, int request_gen, int recv_bytes);
	void RoutePacket(char* packet, Session& session, int request_gen);
	void BroadCastToLobby(char* packet);
	void CreateOpenRoom(char* packet, Session& session, int request_gen);
	void CreateLockRoom(char* packet, Session& session, int request_gen);
	void DeleteRoom(int room_index);
	void ProcessDBResult(DBOverlapped* db_over, Session& session, int request_gen);
	void ProcessRankingResult(DBOverlapped* db_over);
	void RequestLoadRanking();
	void StringToCharBuf(const std::string& str, char* buf, int buf_size);
	std::string CharBufToString(const char* buf, int buf_size);
	void HandlePacket(char* packet, Session& session, int request_gen);
	void SendRoomList(Session& session, int request_gen);
	bool TryJoinRoom(Session& session, int request_gen, int room_gen, const char* room_password);
	int FindRoom(int room_gen);
	int FindUser(int user_id);
	void SendError(Session& session, int request_gen, int error_code);
	bool CheckDuplicateLoginId(const std::string& login_id);
	void FindMatch(Session& session, int request_gen, int max_user);
	void SendLobbyUserList(Session& session, int request_gen);
	void SendFriendList(Session& session, int request_gen);
	void SendRanking(Session& session, int request_gen);
	void SendAddFriendResult(FriendInfo& requester_info, FriendInfo& recver_info);
	void SendDeleteFriendResult(int requester_id, int target_id);
};
