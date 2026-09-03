#pragma once
#pragma once
#include <vector>
#include <array>
#include "Player.h"
#include "Session.h"
#include "define_packets.h"
#include "packet_types.h"
#include "IOCPServer.h"
#include "TetrisRoom.h"

class MultiRoom : public TetrisRoom
{
	int host_id_ = -1;
	int winner_id_ = -1;
public:
	MultiRoom(IOCPServer* server, PublicRoomInitData data);
	MultiRoom(IOCPServer* server, PrivateRoomInitData data);
	~MultiRoom();

	// 공통(오버라이드)
	virtual void HandlePacket(char* packet, const SP<Session>& request_session) override;
	virtual void RemovePlayer(const int player_id) override;
	virtual void SendCreateRoom(const SP<Session>& session) override;
	virtual bool AddHostSession(const SP<Session>& session) override;

	void StartGame(int requester_id);

	void FindNewHost(); // 방장이 나갔을 때 새로운 방장 찾기
	int FindHostIndex(int host_id); // 현재 호스트의 인덱스를 반환
	bool AddPlayerTask(const SP<Session>& new_session, int matching_max_player_count);
	void TogglePlayerReady(int player_id);
	void KickPlayer(int requester_id, int kick_player_id);
	bool ResolveWinner();
	void DistributeGarbageLines();
	int CalculateGarbageLineCount(int cleared_line_count);
	void UpdatePrevPlayerStates();

	void RequestUpdateMatchResult();

private:
	virtual void ProcessSpecificRoomTask(const RoomTask& task) override;
	virtual void ProcessGameTick(std::chrono::steady_clock::time_point tick_time) override;
	int AddPlayer(const SP<Session>& new_session);
};

