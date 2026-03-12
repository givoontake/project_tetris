#include <iostream>
#include <algorithm>
#include "IOCPServer.h"
#include "SingleRoom.h"
#include "MultiRoom.h"

#undef min

IOCPServer::IOCPServer()
{
	for (int i = 0; i < MAX_USER; ++i) {
		users[i] = new Session();
	}

	// load는 객체 복사가 아니라 컨트롤 블록을 가리키는 핸들(shared_ptr)만 복사하는 것, 접근 흐름은 shared_ptr -> controll block(카운터, 실제 객체 포인터 등 존재) -> 실제 객체 이다.
	// 즉 참조 카운트를 늘리는 동작이며 다른 곳에서 객체를 해제해도 안전하게 동작할 수 있도록 한다. 의도된 동작은 아닐 수 있어도 수명은 확실하게 관리된다.
	// shared_ptr의 기본값은 nullptr이므로 초기화는 필요 없다.
	
	//for (int i = 0; i < MAX_ROOM; ++i) {

	//	auto room = rooms[i].load(); 
	//	room = nullptr;
	//}

	//packet_handler = std::make_unique<PacketHandler>(this);
	//for (auto& user : users) {
	//	user = std::make_unique<Session>(packet_handler.get()); // packet_handler는 unique_ptr이므로 get()을 이용해 raw ptr을 넘긴다.
	//}

	WSAStartup(MAKEWORD(2, 2), &wsadata);

	listen_socket = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);
	client_socket = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);
	memset(&server_addr, 0, sizeof(server_addr));
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(PORT_NUM);
	server_addr.sin_addr.S_un.S_addr = INADDR_ANY;

	accept_over.SetOperationType(OP_TYPE::ACCEPT);

}

IOCPServer::~IOCPServer()
{
	for(auto& user : users) {
		delete user;
	}
	for(auto& room : rooms) {
		std::atomic_store(&room, std::shared_ptr<TetrisRoom>{}); // nullptr과 같은 논리
	}
	closesocket(listen_socket);
	closesocket(client_socket);
	db.SetRunning(false);
	WSACleanup();
}

void IOCPServer::HandlePacket(char* packet, Session* request_session) 
{
	// 작업에 필요한 데이터는 락으로 잡고 전송에 필요한 본인 정보만 복사(전송에 필요한 본인 정보를 읽을 때 연결이 끊기면 데이터 레이스 발생 가능)
	switch (packet[2]) {

	case C2S_LOGIN: {
		C2S_LOGIN_PACKET* recv_p = reinterpret_cast<C2S_LOGIN_PACKET*>(packet);

		int id;
		int index;
		std::lock_guard<std::mutex> lock(request_session->GetMutex());
		if (request_session->GetState() == SESS_STATE::LOGIN) {
			id = request_session->GetId();
			index = request_session->GetIndex();
		}
		else return;
		
		// null은 있을수도, 없을수도 있음. 그래서 일단 전체를 받아야함. strnlen(buf, max_size) -> null 직전까지 길이 반환, 안만나면 최대길이 반환
		std::string login_id = CharBufToString(recv_p->login_id, sizeof(recv_p->login_id));
		std::string password = CharBufToString(recv_p->login_password, sizeof(recv_p->login_password));
		Database& repr_db = db; // condition_variable 객체 때문에 복사가 불가능함. 참조로 넘기는 방법밖에 없음
		auto task_login = [&repr_db, id, index, login_id, password]() {
			repr_db.ExecuteLogin(id, index, login_id, password);
			};

		db.Enqueue(task_login);
		//server->GetDB().Enqueue
		//// 우선은 연결 요청이 들어오는 즉시 세션을 사용하도록 함. -> 나중에 반드시 바꿔야함
		//S2C_LOGIN_PACKET send_p;
		//send_p.size = sizeof(S2C_LOGIN_PACKET);
		//send_p.type = S2C_LOGIN;
		//memcpy(send_p.user_name, temp_name, MAX_USER_NAME);
		//if (!memcmp(temp_id, recv_p->user_id, MAX_USER_ID) && !memcmp(temp_password, recv_p->user_password, MAX_USER_PASSWORD)) send_p.id = server->GetSession(user_index)->GetId();	
		//else send_p.id = -1;

		//std::cout << "size: " << recv_p->size << ", type: " << (int)recv_p->type << ", id: " << recv_p->user_id << ", pw: " << recv_p->user_password << std::endl;

		//server->SendToSelf((char*)&send_p, user_index);

		break;
	}

	case C2S_MESSAGE: { 
		C2S_MESSAGE_PACKET* recv_p = reinterpret_cast<C2S_MESSAGE_PACKET*>(packet);
		int msg_size = recv_p->size - sizeof(C2S_MESSAGE_PACKET);
		if (msg_size == 0) return;
		int send_p_size = sizeof(S2C_MESSAGE_PACKET) + msg_size;
		char* send_p = new char[send_p_size];

		std::string nickname;
		int id;
		{
			// 본인 메세지 전송시 연결이 끊겼다면 메시지 무시, 아니라면 필요 정보만 복사
			std::lock_guard<std::mutex> lock(request_session->GetMutex());
			if (request_session->GetState() == SESS_STATE::LOBBY) {
				nickname = request_session->GetInfo().nickname;
				id = request_session->GetId();
			}
			else return;
		}

		// 본인 포함 살아있는 세션에게만 전송 시도
		S2C_MESSAGE_PACKET front_p;
		front_p.size = send_p_size;
		front_p.type = S2C_MESSAGE;
		front_p.id = id;
		StringToCharBuf(nickname, front_p.user_name, sizeof(front_p.user_name));
		memcpy(send_p, &front_p, sizeof(S2C_MESSAGE_PACKET)); // 구조체 부분 복사
		memcpy(send_p + sizeof(S2C_MESSAGE_PACKET), reinterpret_cast<char*>(recv_p) + sizeof(C2S_MESSAGE_PACKET), msg_size); // 가변데이터 복사

		BroadCastLobby(send_p);

		delete[] send_p;

		break;
	}

	case C2S_TEST: {
		// recv_p->size에 구조체 + 가변길이 데이터가 들어있다는 가정하에 구현->나중에 테스트 프로그램 로직도 바꿔야함
		//std::cout << "테스트 패킷 수신" << std::endl;
		C2S_TEST_PACKET* recv_p = reinterpret_cast<C2S_TEST_PACKET*>(packet);
		char* send_p = new char[recv_p->size];
		int msg_size = recv_p->size - sizeof(C2S_TEST_PACKET);
		S2C_TEST_PACKET front_p;
		front_p.size = recv_p->size;
		front_p.type = S2C_TEST;
		front_p.id = request_session->GetId();
		front_p.last_time = recv_p->last_time;
		memcpy(send_p, &front_p, sizeof(S2C_TEST_PACKET)); // 구조체 부분 복사
		memcpy(send_p + sizeof(S2C_TEST_PACKET), reinterpret_cast<char*>(recv_p) + sizeof(C2S_TEST_PACKET), msg_size); // 가변데이터 복사

		BroadCastLobby(send_p);

		delete[] send_p;

		break;
	}

	case C2S_DISCONNECT: {
		request_session->StoreDisconnectFlag(true);
		Disconnect(request_session->GetIndex());
		break;
	}

	case C2S_ADD_OPEN_ROOM: {
		CreateOpenRoom(packet, request_session->GetIndex());
		break;
	}

	case C2S_ADD_LOCK_ROOM: {
		CreateLockRoom(packet, request_session->GetIndex());
		break;
	}

	case C2S_ADD_USER: {
		C2S_ADD_USER_PACKET* add_p = reinterpret_cast<C2S_ADD_USER_PACKET*>(packet);
		break;
	}
	}
}

void IOCPServer::JoinRoom(int user_index, int room_id)
{
	 
}

void IOCPServer::StartServer()
{
	bind(listen_socket, reinterpret_cast<sockaddr*>(&server_addr), sizeof(server_addr));
	listen(listen_socket, SOMAXCONN);
	iocp_handle = CreateIoCompletionPort(INVALID_HANDLE_VALUE, 0, 0, 0);
	CreateIoCompletionPort(reinterpret_cast<HANDLE>(listen_socket), iocp_handle, 9999, 0);
	int addr_size = sizeof(SOCKADDR_IN);
	AcceptEx(listen_socket, client_socket, accept_over.packet_buf, 0, addr_size + 16, addr_size + 16, 0, &accept_over.ex_over.over);
}

void IOCPServer::ProcessGQCS()
{
	while (is_running){
		DWORD transferred_bytes = 0;
		ULONG_PTR key = 0;
		WSAOVERLAPPED* over = nullptr;
		BOOL result = GetQueuedCompletionStatus( // 인자로 넘긴 주소 변수의 값을 채워준다.
			iocp_handle,
			&transferred_bytes,
			&key,
			&over,
			INFINITE);

		ExOverlapped* ex_over = reinterpret_cast<ExOverlapped*>(over);

		if (!result){
			if (ex_over->op_type == OP_TYPE::ACCEPT) std::cout << "Accept Error" << WSAGetLastError() << "\n";
			else { // 클라이언트 강제 종료일 경우
				users[key]->StoreDisconnectFlag(true);
				Disconnect(static_cast<int>(key));
				if (ex_over->op_type == OP_TYPE::SEND) delete ex_over;
			}
			continue;
		}

		// 클라이언트 정상 종료일 경우
		if (transferred_bytes == 0 && ex_over->op_type != OP_TYPE::ACCEPT) {
			users[key]->StoreDisconnectFlag(true);
			Disconnect(static_cast<int>(key));
			if (ex_over->op_type == OP_TYPE::SEND) delete ex_over;
			continue;
		}

		switch (ex_over->op_type) {
		case OP_TYPE::ACCEPT: {
			IOOverlapped* io_over = reinterpret_cast<IOOverlapped*>(ex_over);
			int new_index = GetEmptyUserIndex();
			if (new_index != -1) {
				users[new_index]->InitSession(new_index, GetNewUserId(), client_socket);
				CreateIoCompletionPort(reinterpret_cast<HANDLE>(client_socket), iocp_handle, new_index, 0);
				users[new_index]->RecvPacket(iocp_handle);
				client_socket = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED); // 커널 내 새 소켓을 생성하고 그것을 가리키는 핸들을 받음, 기존 핸들은 이미 initsession 되어 세션 내부에 가지고 있다.
				std::cout << "Session[" << new_index << "] connect/Id: " << users[new_index]->GetId() << std::endl;
			}

			else {
				std::cout << "서버가 혼잡합니다. 연결을 종료합니다.\n";
				closesocket(client_socket);
				client_socket = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED); // WSASocket은 리소스 부족 시 실패할 수 있다. 실패 시 INVALID_SOCKET 반환
			}

			// 리소스 부족으로 실패하면 일단 연결 더 안받는걸로 하자.
			if (client_socket != INVALID_SOCKET) {
				ZeroMemory(&accept_over.ex_over.over, sizeof(accept_over.ex_over.over));
				int addr_size = sizeof(SOCKADDR_IN);
				// AcceptEx도 리소스 부족으로 실패할 수 있다. 
				bool res = AcceptEx(listen_socket, client_socket, accept_over.packet_buf, 0, addr_size + 16, addr_size + 16, 0, &accept_over.ex_over.over);
				if (!res && WSAGetLastError() != ERROR_IO_PENDING) std::cerr << "AcceptEx fail.. " << std::endl;
			}
			break;
		}

		case OP_TYPE::RECV: {
			IOOverlapped* io_over = reinterpret_cast<IOOverlapped*>(ex_over);
			if (io_over->ex_over.operation_id == users[key]->GetId()) {
				ProcessPacket(transferred_bytes, key);
				users[key]->RecvPacket(iocp_handle);
			}
			
			break;
		}

		case OP_TYPE::SEND: {
			IOOverlapped* io_over = reinterpret_cast<IOOverlapped*>(ex_over);
			delete io_over;

			break;
		}
		case OP_TYPE::DELETE_ROOM:
			delete ex_over;
			DeleteRoom(static_cast<int>(key));

			break;
		case OP_TYPE::DB:
			DBOverlapped* db_over = reinterpret_cast<DBOverlapped*>(ex_over);
			ProcessDBResult(db_over, key);
		}
	}
}

void IOCPServer::ProcessPacket(int recv_bytes, int user_index)
{
	//{
	//	std::lock_guard<std::mutex> lock(users[user_index]->GetMutex());
	//	if(users[user_index]->GetState() == NONE)
	//}
	if (recv_bytes + users[user_index]->GetRemainDataSize() > BUF_SIZE) {
		Disconnect(user_index);
		return;
	}

	else users[user_index]->SetRemainDataSize(recv_bytes);

	if (users[user_index]->GetRemainDataSize() < sizeof(short)) return;

	short packet_size = users[user_index]->GetPacketSize(users[user_index]->GetExOver().packet_buf);

	while (users[user_index]->GetRemainDataSize() >= packet_size) // 남아있는 데이터 크기가 실제 처리가능한 데이터 크기이상 존재한다면
	{
		char p_buffer[BUF_SIZE];
		// 패킷 분리: packet_buffer에 복사 후 처리
		memcpy(p_buffer, users[user_index]->GetExOver().packet_buf, packet_size);
		RoutePacket(p_buffer, users[user_index]);

		 // 처리한 패킷은 남은 데이터에서 제거
		users[user_index]->SetRemainDataSize(-packet_size);
		memmove(users[user_index]->GetExOver().packet_buf, users[user_index]->GetExOver().packet_buf + packet_size, users[user_index]->GetRemainDataSize());
		if (users[user_index]->GetRemainDataSize() <= sizeof(short)) break;
		packet_size = users[user_index]->GetPacketSize(users[user_index]->GetExOver().packet_buf);
	}
}

void IOCPServer::RoutePacket(char* packet, Session* request_session)
{
	PrintPacketType(packet[2]);
	switch (request_session->GetState()) {
	case SESS_STATE::NONE:
		return;
	case SESS_STATE::LOGIN:
		HandlePacket(packet, request_session);
		break;
	case SESS_STATE::LOBBY:
		HandlePacket(packet, request_session);
		break;
	case SESS_STATE::ROOM: {
		auto room_ptr = rooms[request_session->GetRoomIndex()].load();
		if (room_ptr) room_ptr->HandlePacket(packet, request_session);
		break;
	}
	}

}

void IOCPServer::BroadCastLobby(char* packet)
{
	for (auto& user : users) {
		if (user->GetState() == SESS_STATE::LOBBY) {// 현재 이 상태는 서버에 연결되어 있는 상태이므로, 나중에 로비, 방에 따라 구분 필요
			user->SendPacket(packet, iocp_handle);
		}
	}
}

void IOCPServer::SendToSelf(char* packet, int self_index)
{
	users[self_index]->SendPacket(packet, iocp_handle);
}

void IOCPServer::CreateOpenRoom(char* packet, int user_index)
{
	C2S_ADD_OPEN_ROOM_PACKET* open_p = reinterpret_cast<C2S_ADD_OPEN_ROOM_PACKET*>(packet);
	OpenRoomInitData data;
	//std::shared_ptr<TetrisRoom> new_room;
	bool is_single;
	if (open_p->max_user == 1) {
		data.max_user = open_p->max_user;
		is_single = true;
	}
		
	else if (open_p->max_user == 2 || open_p->max_user == 5) {
		data.max_user = open_p->max_user;
		is_single = false;
	}
		
	else return;
	memcpy(data.room_name, open_p->room_name, sizeof(data.room_name));
	data.room_id = GetNewRoomId();
	
	std::shared_ptr<TetrisRoom> new_room;
	
	for (int i = 0; i < MAX_ROOM; ++i) {
		if (rooms[i].load() == nullptr) {
			data.room_index = i;
			if (is_single) new_room = std::make_shared<SingleRoom>(this, users[user_index], data);
			else new_room = std::make_shared<MultiRoom>(this, users[user_index], data);
			std::shared_ptr<TetrisRoom> expected = nullptr;
			if (std::atomic_compare_exchange_strong(&rooms[i], &expected, new_room)) {
				//std::cout << "Open Room created, Room index: " << i << ", Room id: " << room_id << std::endl;
				return;
			}
		}
	}

	// 나중에 방 못찾으면 추후 처리 필요
}

void IOCPServer::CreateLockRoom(char* packet, int user_index)
{
	C2S_ADD_LOCK_ROOM_PACKET* lock_p = reinterpret_cast<C2S_ADD_LOCK_ROOM_PACKET*>(packet);
	LockRoomInitData data;
	//std::shared_ptr<TetrisRoom> new_room;
	bool is_single;
	if (lock_p->max_user == 1) {
		data.max_user = lock_p->max_user;
		is_single = true;
	}

	else if (lock_p->max_user == 2 || lock_p->max_user == 5) {
		data.max_user = lock_p->max_user;
		is_single = false;
	}

	else return;
	memcpy(data.room_name, lock_p->room_name, sizeof(data.room_name));
	memcpy(data.room_password, lock_p->room_password, sizeof(data.room_password));
	data.room_id = GetNewRoomId();

	std::shared_ptr<TetrisRoom> new_room;

	for (int i = 0; i < MAX_ROOM; ++i) {
		if (rooms[i].load() == nullptr) {
			data.room_index = i;
			if (is_single) new_room = std::make_shared<SingleRoom>(this, users[user_index], data);
			else new_room = std::make_shared<MultiRoom>(this, users[user_index], data);
			std::shared_ptr<TetrisRoom> expected = nullptr;
			if (std::atomic_compare_exchange_strong(&rooms[i], &expected, new_room)) {
				//std::cout << "Open Room created, Room index: " << i << ", Room id: " << room_id << std::endl;
				return;
			}
		}
	}
}

void IOCPServer::DeleteRoom(int room_index)
{
	rooms[room_index].store(nullptr);
	std::cout << "Room deleted, Room index: " << room_index << std::endl;
}

int IOCPServer::GetNewUserId()
{
	return user_id_generator.fetch_add(1) + 1; // fetch_add는 값을 실제로 원자적으로 증가시키지만, 반환하는 것은 증가 이전의 값
}

int IOCPServer::GetNewRoomId()
{
	return room_id_generator.fetch_add(1) + 1;
}

int IOCPServer::GetEmptyUserIndex()
{
	for (int i = 0; i < MAX_USER; ++i) {
		if (users[i]->GetState() == SESS_STATE::NONE) {
			if (users[i]->TryChangeState(SESS_STATE::NONE, SESS_STATE::LOGIN)) {
				return i;
			}
		}
	}

	return -1;
}

int IOCPServer::GetEmptyRoomIndex()
{
	for (int i = 0; i < MAX_ROOM; ++i) {
		auto room = rooms[i].load();
		if (!room) return i; // 외부에서 받은 인덱스로 cas를 시도함.
	}

	return -1;
}

void IOCPServer::Disconnect(int user_index)
{
	if (users[user_index]->GetState() == SESS_STATE::NONE) return; // 이미 끊김->또 send -> send 실패 -> PQCS -> Disconnect 무한루프 방지

	if (users[user_index]->TryChangeDisconnectFlag(true, false)) {
		if (users[user_index]->GetState() == SESS_STATE::ROOM) {
			auto room = rooms[users[user_index]->GetRoomIndex()].load();
			if (room) room->DeleteUser(users[user_index]->GetId());
		}

		std::cout << "Session index[" << users[user_index]->GetIndex() << "] disconnect/Id: " << users[user_index]->GetId() << std::endl;
		users[user_index]->ClearSession();
	}
	
	//S2C_DISCONNECT_PACKET p;
	//p.size = sizeof(S2C_DISCONNECT_PACKET);
	//p.type = S2C_DISCONNECT;
	//SendToSelf(reinterpret_cast<char*>(&p), user_index);

}

void IOCPServer::ProcessDBResult(DBOverlapped* db_over, int user_index)
{
	// DB 작업 이후 결과를 세션에 통지하기 전에, 세션이 종료되었을 수 있다.
	// 만약 세션이 즉시 재사용된다면, 우연히 세션을 초기화하는 과정에서 아이디가 바뀌기 전에 다른 부분이 먼저 변경되었을 가능성이 있다.
	// DB 작업만 문제가 되는게 아니다. 일반 IO도 오퍼레이션 아이디를 넣기는 하지만, 위와 같이 다른 세션인데 우연히 아이디는 바뀌지 않았을 가능성이 있다.
	// 따라서 세션의 초기화는 뮤텍스로 처리해야 한다. 클리어는 굳이 뮤텍스로 처리하지 않아도 될 것 같다. 세션이 비었다는 상태를 클리어 맨 마지막에 저장하면 즉시 재사용된다고 해도 문제는 생기지 않는다.

	if (users[user_index]->GetId() != db_over->ex_over.operation_id) return;

	switch (db_over->type) {
	case DBOperationType::LOGIN: {
		S2C_LOGIN_PACKET login_p;
		ZeroMemory(&login_p, sizeof(login_p));
		login_p.size = sizeof(S2C_LOGIN_PACKET);
		login_p.type = S2C_LOGIN;
		if (db_over->ok) {
			if (db_over->result_data) { // nullptr이 아니면, 즉 포인터가 존재하면
				users[user_index]->InitDBInfo(static_cast<DBResultLogin*>(db_over->result_data.get()));
				users[user_index]->StoreState(SESS_STATE::LOBBY);
				login_p.id = users[user_index]->GetId();
				login_p.max_score = users[user_index]->GetInfo().max_score;
				login_p.win_count = users[user_index]->GetInfo().win_count;
				login_p.lose_count = users[user_index]->GetInfo().lose_count;
				StringToCharBuf(users[user_index]->GetInfo().nickname, login_p.nickname, sizeof(login_p.nickname));
			}
		}
		else {
			login_p.id = -1; // 근데 로그인 실패일경우 나머지 패킷도 다같이 가는건 낭비같은데.. 결국 로그인 성공과 세션 데이터 전송은 분리 해야할듯
		}
		users[user_index]->SendPacket(reinterpret_cast<char*>(&login_p), iocp_handle);
		break;
	}
	case DBOperationType::UPDATE_SCORE:
		if (db_over->ok) {
			// void*는 사용할 때 타입을 명시해야함(컴파일러가 알아들을 수 있도록)
			// 보이드 유니크 포인터인 db_over->info를 get 함수로 raw 포인터를 가져와 사용할 포인터로 static_cast, void* <-> T* 간에는 static_cast가 허용되고, void*에는 T*를 대입할 수 있다.
			DBResultUpdateScore* res = static_cast<DBResultUpdateScore*>(db_over->result_data.get());
			users[user_index]->GetInfo().max_score = res->max_score;
			S2C_UPDATE_SCORE_PACKET us_p;
			us_p.size = sizeof(S2C_UPDATE_SCORE_PACKET);
			us_p.type = S2C_UPDATE_SCORE;
			us_p.max_score = res->max_score;
			users[user_index]->SendPacket(reinterpret_cast<char*>(&us_p), iocp_handle);
		} 
		break;

	case DBOperationType::UPDATE_MATCH_RESULT:
		if (db_over->ok) {
			DBResultUpdateMatchResult* res = static_cast<DBResultUpdateMatchResult*>(db_over->result_data.get());
			if (res->is_winner) ++users[user_index]->GetInfo().win_count;
			else ++users[user_index]->GetInfo().lose_count;

			S2C_MATCH_RECORD_PACKET record_p;
			record_p.size = sizeof(S2C_MATCH_RECORD_PACKET);
			record_p.type = S2C_MATCH_RECORD;
			record_p.win_count = users[user_index]->GetInfo().win_count;
			record_p.lose_count = users[user_index]->GetInfo().lose_count;
			users[user_index]->SendPacket(reinterpret_cast<char*>(&record_p), iocp_handle);
		}
		break;
	}

	delete db_over;
}

void IOCPServer::StringToCharBuf(const std::string& str, char* buf, int buf_size)
{
	ZeroMemory(buf, buf_size);
	int copy_size = std::min(str.size(), static_cast<size_t>(buf_size));
	memcpy(buf, str.data(), copy_size);
}

std::string IOCPServer::CharBufToString(const char* buf, int buf_size)
{
	int copy_size = strnlen(buf, buf_size);
	std::string str(buf, copy_size);
	return str;
}

