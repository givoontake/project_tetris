#pragma once
#include <mutex>
#include <unordered_map>
#include <vector>
#include "types.h"

class TetrisRoom;

class ActiveRoomManager
{
	std::unordered_map<int, WP<TetrisRoom>> active_rooms_;
	mutable std::mutex active_rooms_mutex_;

public:
	void AddRoom(int room_gen, const SP<TetrisRoom>& room);
	void RemoveRoom(int room_gen, const SP<TetrisRoom>& room);
	SP<TetrisRoom> FindRoomByGen(int room_gen);
	std::vector<SP<TetrisRoom>> GetActiveRoomsSnapshot() const;
	void Clear();
};
