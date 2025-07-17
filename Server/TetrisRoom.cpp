#include "TetrisRoom.h"

TetrisRoom::TetrisRoom()
{
	
}

TetrisRoom::~TetrisRoom()
{

}

void TetrisRoom::AddUser(C2S_ADD_USER_PACKET& p)
{
	if (users.size() >= max_user) {

		room_mutex.lock();
		RoomSession new_user(p);
		users.emplace_back(p);
		if (room_state == EMPTY) {
			room_state = LOBBY;
			host_id = p.id;
		}
		room_mutex.unlock();
		
		// 추가 브로드캐스트
	}

	else {
		// 추가 실패 패킷 전송(본인에게만)
	}
}

void TetrisRoom::DeleteUser(C2S_DELETE_USER_PACKET& p)
{
	for (int i = 0; i < max_user; ++i) {
		if (users[i].GetId() == p.id) {
			room_mutex.lock();
			users.erase(users.begin());
			// 삭제 브로드캐스트, 새 방장 여부도 포함해야함.

			if (users.empty()) {
				room_state = EMPTY;
			}

			room_mutex.unlock();
		}

	}
}

void TetrisRoom::InitRoom(const char* name, int user_id, int _max_user)
{
	host_id = user_id;
	max_user = _max_user; // 이래서 맴버 변수에 m을 붙이나..
	users.reserve(max_user);
}
