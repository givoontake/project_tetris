#pragma once
#include <memory>
#include <utility>
#include <vector>
#include "DBResult.h"
#include "ExOverlapped.h"

enum class SessionTaskType { PACKET, DB_RESULT, ADD_FRIEND, DELETE_FRIEND, FRIEND_REQUEST, DISCONNECT };
enum class SessionTaskProcessResult { CONTINUE, RESTORE_REMAINING, DISCARD_REMAINING };

struct SessionTask
{
	SessionTaskType task_type;
	SessionKey session_key;

	explicit SessionTask(SessionTaskType task_type) : task_type(task_type) {}
	virtual ~SessionTask() = default;
};

struct SessionPacketTask final : SessionTask
{
	std::vector<char> packet;

	SessionPacketTask(const char* packet_data, int packet_size)
		: SessionTask(SessionTaskType::PACKET), packet(packet_data, packet_data + packet_size) {}
};

struct SessionDisconnectTask final : SessionTask
{
	SessionDisconnectTask() : SessionTask(SessionTaskType::DISCONNECT) {}
};

struct SessionDBResultTask final : SessionTask
{
	std::unique_ptr<DBOverlapped> db_over;

	explicit SessionDBResultTask(std::unique_ptr<DBOverlapped> db_over)
		: SessionTask(SessionTaskType::DB_RESULT), db_over(std::move(db_over)) {}
};

struct SessionAddFriendTask final : SessionTask
{
	FriendInfo friend_info;

	explicit SessionAddFriendTask(FriendInfo friend_info)
		: SessionTask(SessionTaskType::ADD_FRIEND), friend_info(std::move(friend_info)) {}
};

struct SessionDeleteFriendTask final : SessionTask
{
	int target_player_id;

	explicit SessionDeleteFriendTask(int target_player_id)
		: SessionTask(SessionTaskType::DELETE_FRIEND), target_player_id(target_player_id) {}
};

struct SessionFriendRequestTask final : SessionTask
{
	FriendInfo requester_info;

	explicit SessionFriendRequestTask(FriendInfo requester_info)
		: SessionTask(SessionTaskType::FRIEND_REQUEST), requester_info(std::move(requester_info)) {}
};
