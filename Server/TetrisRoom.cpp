#include "TetrisRoom.h"

TetrisRoom::TetrisRoom(IOCPServer* server) : server(server), room_handler(this, server)
{
	
}

TetrisRoom::~TetrisRoom()
{

}

void TetrisRoom::InitRoom(char* packet, Session* session) // 네트워크 절약을 위해 비밀번호 포함 여부를 다르게 하여 패킷을 두 개로 구분, 같은 역할이므로 하나의 함수로 받아 케이스로 처리
{
	switch (packet[2]) {
	case C2S_ADD_OPEN_ROOM: {
		C2S_ADD_OPEN_ROOM_PACKET* p = reinterpret_cast<C2S_ADD_OPEN_ROOM_PACKET*>(packet);
		if (!(p->max_user == 1 || p->max_user == 2 || p->max_user == 5)) break;
		host_id = p->id;
		max_user = p->max_user;
		// resize는 만약 기존 벡터 메모리가 부족할 경우 새 메모리를 할당하고 기존 메모리 내용을 복사하는데, 여기서 아토믹 복사 불가 문제가 발생할 수 있음
		room_users.reserve(p->max_user); // 미리 메모리를 할당하고 객체를 채우면 문제 x
		for (int i = 0; i < p->max_user; ++i){
			room_users.emplace_back(); 
		}
		memcpy(room_name, p->room_name, sizeof(room_name));
		is_password = false;
		AddUser(session);
		break;
	}

	case C2S_ADD_LOCK_ROOM:{
		C2S_ADD_LOCK_ROOM_PACKET* p = reinterpret_cast<C2S_ADD_LOCK_ROOM_PACKET*>(packet);
		if (!(p->max_user == 1 || p->max_user == 2 || p->max_user == 5)) break;
		host_id = p->id;
		max_user = p->max_user;
		room_users.reserve(p->max_user);
		for (int i = 0; i < p->max_user; ++i) {
			room_users.emplace_back();
		}
		memcpy(room_name, p->room_name, sizeof(room_name));
		is_password = true;
		memcpy(room_password, p->room_password, sizeof(room_password));
		AddUser(session);
		break;
	}
	}
}

void TetrisRoom::AddUser(Session* new_session)
{
	//C2S_ADD_USER_PACKET* recv_p = reinterpret_cast<C2S_ADD_USER_PACKET*>(packet);

	bool b_send = false;
	for(auto& r_user : room_users){
		if (r_user.GetInUse()) continue;
			
		else {
			r_user.InitSession(new_session);
			S2C_ADD_USER_PACKET p;
			p.size = sizeof(S2C_ADD_USER_PACKET);
			p.type = S2C_ADD_USER;
			// p.name = 세션에 이름 변수 추가 필요
			p.id = new_session->GetIndex();
			p.is_add = true;
			Broadcast(reinterpret_cast<char*>(&p), server->GetHandle());

			return;
		}	

	}

	// 성공과 실패에 따라 패킷을 나눌까? 사실 방이 다 차 있다면 클라이언트 수준에서 송신 자체를 막아야 할 것 같기는 한데..
	// 아니지. 클라에서 실제로 방이 빈 것으로 보였어도, 누군가가 먼저 차지했다면 그건 알 수가 없으니까 처리가 필요
	S2C_ADD_USER_PACKET p;
	p.size = sizeof(S2C_ADD_USER_PACKET);
	p.type = S2C_ADD_USER;
	// p.name = 세션에 이름 변수 추가 필요
	p.id = new_session->GetIndex();
	p.is_add = false;
	new_session->SendPacket(reinterpret_cast<char*>(&p), server->GetHandle()); // 방이 꽉 찼을 경우 본인에게만 실패 전송
}

void TetrisRoom::DeleteUser(const int id)
{
	for (auto& r_user : room_users){
		if (r_user.GetSession()->GetIndex() == id) { // 삭제할 아이디 검색
			//room_mutex.lock();
			r_user.SetUse(true, false);
			r_user.ClearSession(); // 해당 아이디 세션 정리

			S2C_DELETE_USER_PACKET p;
			p.size = sizeof(S2C_DELETE_USER_PACKET);
			p.type = S2C_DELETE_USER;
			p.id = id;
			if (p.id == host_id) { // 새 방장 여부에 따른 처리
				int new_host_id = FindNewHost();
				if (new_host_id == -1) { // 현재 방에 아무도 없으면
					SetRoomState(EMPTY);
				}
				p.new_host_id = new_host_id;
			}
			Broadcast(reinterpret_cast<char*>(&p), server->GetHandle());

			break;
			//room_mutex.unlock();
		}
	}
}

void TetrisRoom::ReadyUser(const C2S_READY_PACKET& packet)
{
	if (host_id == packet.id) return;

	for (auto& r_user : room_users){
		if (r_user.GetSession()->GetIndex() == packet.id) { // 레디 상태 변화
			r_user.SetIsReady(packet.is_ready);

			S2C_READY_PACKET p;
			p.size = sizeof(S2C_READY_PACKET);
			p.type = S2C_READY;
			p.id = r_user.GetSession()->GetIndex();
			p.is_ready = r_user.GetIsReady();
			Broadcast(reinterpret_cast<char*>(&p), server->GetHandle());
			break;
		}
	}

}


void TetrisRoom::KickUser(const C2S_KICK_PACKET& packet)
{
	if (packet.id != host_id) return;

	for (auto& r_user : room_users) {
		if (r_user.GetSession()->GetIndex() == packet.kick_user_id) { // 삭제할 아이디 검색
			r_user.SetUse(true, false);
			r_user.ClearSession(); // 해당 아이디 세션 정리

			S2C_KICK_PACKET p;
			p.size = sizeof(S2C_KICK_PACKET);
			p.type = S2C_KICK;
			p.kick_user_id = packet.kick_user_id;
			Broadcast(reinterpret_cast<char*>(&p), server->GetHandle());

			break;
		}
	}
}

void TetrisRoom::StartGame(const C2S_START_PACKET& packet)
{
	if (packet.id != host_id) return;

	int host_index = -1;
	for (int i = 0; i < max_user; i++){
		if (room_users[i].GetSession()->GetIndex() == host_id) {
			host_index = i;
			break;
		}
	}

	if(host_index == -1) return; // host_id가 논리적으로는 존재해야 하지만.. 버그 예외처리

	int ready_user_count = 0;
	S2C_START_PACKET p;

	for(auto& r_user : room_users){
		if (!r_user.GetInUse()) continue; // 사용 중이지 않은 인덱스는 건너뜀
		if (r_user.GetSession()->GetIndex() == host_id) continue; // 방장은 건너뜀

		if (!r_user.GetIsReady()) { // 방에 있는데 레디가 안된 사람이 있으면 시작 불가			
			p.size = sizeof(S2C_START_PACKET);
			p.type = S2C_START;
			p.is_start = false;

			room_users[host_index].GetSession()->SendPacket(reinterpret_cast<char*>(&p), server->GetHandle()); // 시작 불가는 방장에게만 보내면 됨
			return;
		}
		else ++ready_user_count;
	}

	if (ready_user_count == 0) { // 방장만 존재하면 당연히 시작 불가
		p.size = sizeof(S2C_START_PACKET);
		p.type = S2C_START;
		p.is_start = false;

		room_users[host_index].GetSession()->SendPacket(reinterpret_cast<char*>(&p), server->GetHandle()); // 시작 불가는 방장에게만 보내면 됨
		return;
	}

	// 모든 조건 통과->게임 시작
	room_state = PLAY; 

	// 테트리스 게임 중에 들어오는 패킷은 또 따로 분리하고 싶기는 한데..
	InitGame();

	p.size = sizeof(S2C_START_PACKET);
	p.type = S2C_START;
	p.is_start = true;
	Broadcast(reinterpret_cast<char*>(&p), server->GetHandle());
}

void TetrisRoom::Broadcast(char* packet, const HANDLE iocp_handle)
{
	for (auto& r_user : room_users) {
		if (r_user.GetInUse()) {
			r_user.GetSession()->SendPacket(packet, iocp_handle);
		}
	}
}

void TetrisRoom::SendToSelf(char* packet, int self_id, const HANDLE iocp_handle)
{
	for (auto& r_user : room_users) {
		if (r_user.GetInUse() && r_user.GetSession()->GetIndex() == self_id) {
			r_user.GetSession()->SendPacket(packet, iocp_handle);
			break;
		}
	}
}

void TetrisRoom::InitGame()
{
	for (auto& r_user : room_users) { 
		if (!r_user.GetInUse()) continue;
		r_user.GetTetris().ClearBoard();
	}
}


void TetrisRoom::SetRoomId(int room_index)
{
	room_id = room_index;
}

void TetrisRoom::SetRoomState(ROOM_STATE new_state)
{
	room_state = new_state;
}

int TetrisRoom::FindNewHost()
{
	for (auto& r_user : room_users) {
		if (r_user.GetInUse()) return r_user.GetSession()->GetIndex();
	}
	return -1;
}

