#include "ActiveRoomManager.h"
#include "TetrisRoom.h"
#include <algorithm>

void ActiveRoomManager::AddRoom(int room_gen, const SP<TetrisRoom>& room)
{
	if (!room) return;
	std::lock_guard<std::mutex> lock(active_rooms_mutex_);
	active_rooms_[room_gen] = room;
}

void ActiveRoomManager::RemoveRoom(int room_gen, const SP<TetrisRoom>& room)
{
	std::lock_guard<std::mutex> lock(active_rooms_mutex_);
	auto it = active_rooms_.find(room_gen);
	if (it == active_rooms_.end()) return;
	auto active_room = it->second.lock();
	if (active_room && active_room != room) return;
	active_rooms_.erase(it);
}

SP<TetrisRoom> ActiveRoomManager::FindRoom(int room_gen)
{
	std::lock_guard<std::mutex> lock(active_rooms_mutex_);
	auto it = active_rooms_.find(room_gen);
	if (it == active_rooms_.end()) return nullptr;
	auto room = it->second.lock();
	if (!room) {
		active_rooms_.erase(it);
		return nullptr;
	}
	if (room->GetRoomGen() != room_gen) {
		active_rooms_.erase(it);
		return nullptr;
	}
	return room;
}

std::vector<SP<TetrisRoom>> ActiveRoomManager::GetActiveRoomsSnapshot() const
{
	std::lock_guard<std::mutex> lock(active_rooms_mutex_);
	std::vector<SP<TetrisRoom>> snapshot;
	snapshot.reserve(active_rooms_.size());
	for (const auto& room : active_rooms_) {
		auto room_ptr = room.second.lock();
		if (!room_ptr) continue;
		if (room_ptr->GetRoomGen() != room.first) continue;
		snapshot.push_back(room_ptr);
	}
	std::sort(snapshot.begin(), snapshot.end(), [](const SP<TetrisRoom>& lhs, const SP<TetrisRoom>& rhs) {
		return lhs->GetRoomIndex() < rhs->GetRoomIndex();
		});
	return snapshot;
}

void ActiveRoomManager::Clear()
{
	std::lock_guard<std::mutex> lock(active_rooms_mutex_);
	active_rooms_.clear();
}
