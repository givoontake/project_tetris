#pragma once
#pragma once
#include <vector>
#include <array>
#include <mutex>
#include "RoomSession.h"
#include "Session.h"
#include "define.h"
#include "packet_type.h"
#include "RoomPacketHandler.h"
#include "IOCPServer.h"
#include "TetrisRoom.h"

class MultiRoom : public TetrisRoom
{
	int host_id;
	int winner_id = -1;
public:
	MultiRoom(IOCPServer* server, Session& session, OpenRoomInitData data);
	MultiRoom(IOCPServer* server, Session& session, LockRoomInitData data);
	~MultiRoom();

	// 공통(오버라이드)
	virtual void HandlePacket(char* packet, Session& request_session) override;
	virtual void ProcessPlayTasks() override;
	virtual void DeleteUser(const int id) override;
	virtual void SendCreateRoom(Session& session) override;

	void StartGame(int request_user_id);

	void FindNewHost(); // 방장이 나갔을 때 새로운 방장 찾기
	int FindHostIndex(int host_id); // 현재 호스트의 인덱스를 반환
	int AddUser(Session& new_session, int request_gen, const char* room_password);
	void ReadyUser(int id);
	void KickUser(int id, int kick_user_id);
	bool FindWinner();
	void CalcAttackLine();
	int GetGarbageLinesFromAttack(int cleared_line_num);
	void UpdatePrevUsersState();

	void RequestUpdateMatchResult();
};

