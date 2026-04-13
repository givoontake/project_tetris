#include "ActiveRoomManager.h"

void ActiveRoomManager::AddRoom(int room_gen, int room_index)
{
	std::lock_guard<std::mutex> lock(active_rooms_mutex);
	active_rooms[room_gen] = room_index;
}

void ActiveRoomManager::RemoveRoom(int room_gen, int room_index)
{
	std::lock_guard<std::mutex> lock(active_rooms_mutex);
	auto it = active_rooms.find(room_gen);
	if (it == active_rooms.end()) return;
	if (it->second != room_index) return;
	active_rooms.erase(it);
}

int ActiveRoomManager::FindRoomIndex(int room_gen) const
{
	std::lock_guard<std::mutex> lock(active_rooms_mutex);
	auto it = active_rooms.find(room_gen);
	if (it == active_rooms.end()) return -1;
	return it->second;
}

void ActiveRoomManager::Clear()
{
	std::lock_guard<std::mutex> lock(active_rooms_mutex);
	active_rooms.clear();
}
