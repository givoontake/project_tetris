#pragma once
#pragma once
#include <vector>
#include <array>
#include "RoomSession.h"
#include "Session.h"
#include "define_packets.h"
#include "packet_types.h"
#include "IOCPServer.h"
#include "TetrisRoom.h"

class MultiRoom : public TetrisRoom
{
	int host_id = -1;
	int winner_id = -1;
public:
	MultiRoom(IOCPServer* server, OpenRoomInitData data);
	MultiRoom(IOCPServer* server, LockRoomInitData data);
	~MultiRoom();

	// 공통(오버라이드)
	virtual void HandlePacket(char* packet, const SP<Session>& request_session) override;
	virtual void DeleteUser(const int id) override;
	virtual void SendCreateRoom(const SP<Session>& session) override;
	virtual bool AddHostSession(const SP<Session>& session) override;

	void StartGame(int request_user_id);

	void FindNewHost(); // 방장이 나갔을 때 새로운 방장 찾기
	int FindHostIndex(int host_id); // 현재 호스트의 인덱스를 반환
	bool AddUserTask(const SP<Session>& new_session, int matching_max_user);
	void ReadyUser(int id);
	void KickUser(int id, int kick_user_id);
	bool FindWinner();
	void CalcAttackLine();
	int GetGarbageLinesFromAttack(int cleared_line_num);
	void UpdatePrevUsersState();

	void RequestUpdateMatchResult();

private:
	virtual void ProcessSpecificRoomTask(const RoomTaskInfo& task) override;
	virtual void ProcessGameTick() override;
	int AddUser(const SP<Session>& new_session);
};

