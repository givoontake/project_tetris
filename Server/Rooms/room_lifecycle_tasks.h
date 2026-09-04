#pragma once
#include <memory>
#include <string>
#include <vector>
#include "ExOverlapped.h"

enum class RoomLifecycleTaskType { CREATE_PUBLIC, CREATE_PRIVATE, JOIN_ROOM, LOBBY_TRANSITION_RESULT, DELETE_ROOM };

struct RoomLifecycleTask
{
	RoomLifecycleTaskType task_type;
	SessionKey session_key;
	std::vector<char> packet;
	std::string room_password;
	RoomExitType exit_type = RoomExitType::LEAVE;
	int room_index = -1;
	int room_gen = -1;
	int matching_max_player_count = -1;
	int result = SUCCESS;

	RoomLifecycleTask(RoomLifecycleTaskType task_type, SessionKey session_key)
		: task_type(task_type), session_key(session_key) {}

	explicit RoomLifecycleTask(int room_index)
		: task_type(RoomLifecycleTaskType::DELETE_ROOM), room_index(room_index) {}
};
