#pragma once
#include <mutex>
#include <unordered_map>
#include <vector>
#include "Types.h"

class TetrisRoom;

class ActiveRoomManager
{
	std::unordered_map<int, WP<TetrisRoom>> active_rooms;
	mutable std::mutex active_rooms_mutex;

public:
	void AddRoom(int room_gen, const SP<TetrisRoom>& room);
	void RemoveRoom(int room_gen, const SP<TetrisRoom>& room);
	SP<TetrisRoom> FindRoom(int room_gen);
	std::vector<SP<TetrisRoom>> GetActiveRoomsSnapshot() const;
	void Clear();
};
