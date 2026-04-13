#pragma once
#include <mutex>
#include <unordered_map>

class ActiveRoomManager
{
	std::unordered_map<int, int> active_rooms;
	mutable std::mutex active_rooms_mutex;

public:
	void AddRoom(int room_gen, int room_index);
	void RemoveRoom(int room_gen, int room_index);
	int FindRoomIndex(int room_gen) const;
	void Clear();
};
