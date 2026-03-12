#pragma once
#include <WinSock2.h>
#include <MSWSock.h>
#include <array>
#include <atomic>
#include <memory>
#include "ExOverlapped.h"
#include "Session.h"
#include "PacketHandler.h"
#include "MQueue.h"
#include "TetrisRoom.h"
#include "Atomic.h"
#include "Database.h"
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
	std::atomic<int> user_id_generator = -1;
	std::atomic<int> room_id_generator = -1;
	std::atomic<long long> tick_count = 0;
	std::array<Session*, MAX_USER> users;
	std::array<std::atomic<std::shared_ptr<TetrisRoom>>, MAX_ROOM> rooms;
	
	// 변수-> 컨테이너 생성 시 객체 생성자에 인자 넣는게 안된다.
	// 포인터 -> 생성자에서 인자 넣고 동적할당 하면 된다.

	bool is_running = true;
	
public:
	IOCPServer();
	~IOCPServer();

	//getters
	//std::array<std::unique_ptr<Session>, MAX_USER>& GetSessionList() { return users; };
	//std::array<std::unique_ptr<TetrisRoom>, MAX_ROOM>& GetRoomList() { return rooms; };
	int GetEmptyUserIndex();
	int GetEmptyRoomIndex();
	int GetNewUserId();
	int GetNewRoomId();
	bool GetRunning() const { return is_running; }
	HANDLE GetHandle() const { return iocp_handle; }
	Session* GetSession(int user_index) const { return users[user_index]; }
	long long GetTickCount() const { return tick_count.load(); }
	std::shared_ptr<TetrisRoom> GetRoom(int room_index) const { return std::atomic_load(&rooms[room_index]); }
	Database& GetDB() { return db; }

	void AddTickCount() { tick_count.fetch_add(1); }

	//virtual MQueue& GetTaskQueue() override;

	void Disconnect(int user_index);
	void StartServer();
	void ProcessGQCS();
	void ProcessPacket(int recv_bytes, int user_index);
	void RoutePacket(char* packet, Session* request_session);
	void BroadCastLobby(char* packet);
	//void BroadCastRoom(char* packet, int room_id);
	void SendToSelf(char* packet, int self_index);
	void CreateOpenRoom(char* packet, int user_index); // 컨테이너 조작이 필요한 패킷은 서버에 함수를 일단 만들어 두고 처리
	void CreateLockRoom(char* packet, int user_index);
	void DeleteRoom(int room_index);
	void ProcessDBResult(DBOverlapped* db_over, int user_index);
	void StringToCharBuf(const std::string& str, char* buf, int buf_size);
	std::string CharBufToString(const char* buf, int buf_size);
	void HandlePacket(char* packet, Session* request_session);
	void JoinRoom(int user_index, int room_id);
};
