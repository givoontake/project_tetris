#pragma once
#pragma once
#include <vector>
#include <array>
#include "Player.h"
#include "Session.h"
#include "TetrisServer.h"
#include "TetrisRoom.h"

class MultiRoom : public TetrisRoom
{
	int host_id_ = -1;
	int winner_id_ = -1;
public:
	MultiRoom(TetrisServer* server, PublicRoomInitData data);
	MultiRoom(TetrisServer* server, PrivateRoomInitData data);
	~MultiRoom();

	// 공통(오버라이드)
	virtual void HandlePacket(char* packet, Session& request_session) override;
	virtual void RemovePlayer(SessionKey session_key) override;
	virtual void SendCreateRoom(Session& session) override;
	virtual bool AddHostSession(Session& session, SessionKey session_key) override;
	virtual int AddPlayer(Session& new_session, SessionKey session_key, const std::string& room_password) override;

	void StartGame(int requester_id);

	void FindNewHost(); // 방장이 나갔을 때 새로운 방장 찾기
	int FindHostIndex(int host_id); // 현재 호스트의 인덱스를 반환
	void TogglePlayerReady(int player_id);
	bool KickPlayer(int requester_id, int kick_player_id);
	bool ResolveWinner();
	void DistributeGarbageLines();
	int CalculateGarbageLineCount(int cleared_line_count);
	void UpdatePrevPlayerStates();

	void RequestUpdateMatchResult();

private:
	virtual void CompletePlayerRemoval(SessionKey session_key, RoomExitType exit_type) override;
	virtual bool ProcessSpecificRoomTask(const RoomTask& task) override;
	virtual void ProcessGameTick(long long tick_time_ms) override;
};
