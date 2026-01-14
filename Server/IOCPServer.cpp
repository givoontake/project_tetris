#include <iostream>
#include <algorithm>
#include "IOCPServer.h"

#undef min

IOCPServer::IOCPServer() : handler(this)
{
	for (int i = 0; i < MAX_USER; ++i) {
		users[i] = new Session();
	}

	for (int i = 0; i < MAX_ROOM; ++i){
		rooms[i] = new TetrisRoom(this);
		rooms[i]->SetRoomId(i); // 방 아이디를 정해줄 부분이 마땅치 않아 서버 생성 시 만들기로 했다..
	}

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
	closesocket(listen_socket);
	closesocket(client_socket);
	db.SetRunning(false);
	WSACleanup();
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
				Disconnect(static_cast<int>(key));
				if (ex_over->op_type == OP_TYPE::SEND) delete ex_over;
			}
			continue;
		}

		// 클라이언트 정상 종료일 경우
		if (transferred_bytes == 0 && ex_over->op_type != OP_TYPE::ACCEPT) {
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
				client_socket = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);
				std::cout << "Session[" << new_index << "] connect/Id: " << users[new_index]->GetId() << std::endl;

				//S2C_TEST_LOGIN_PACKET send_p;
				//send_p.size = sizeof(S2C_TEST_LOGIN_PACKET);
				//send_p.type = S2C_TEST_LOGIN;
				//send_p.id = users[new_index]->GetId();
				//SendToSelf((char*)&send_p, new_index);

				//S2C_LOGIN_PACKET send_p;
				//send_p.size = sizeof(S2C_LOGIN_PACKET);
				//send_p.type = S2C_LOGIN;
				//send_p.id = users[new_index]->GetId();
				//SendToSelf((char*)&send_p, new_index);
			}
			else std::cout << "서버가 혼잡합니다. 연결을 종료합니다.\n";

			ZeroMemory(&accept_over.ex_over.over, sizeof(accept_over.ex_over.over));
			int addr_size = sizeof(SOCKADDR_IN);
			AcceptEx(listen_socket, client_socket, accept_over.packet_buf, 0, addr_size + 16, addr_size + 16, 0, &accept_over.ex_over.over);
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
		case OP_TYPE::DB:
			DBOverlapped* db_over = reinterpret_cast<DBOverlapped*>(ex_over);
			ProcessDB(db_over, key);
		}
	}
}

void IOCPServer::ProcessPacket(int recv_bytes, int user_index)
{
	if (users[user_index]->GetState() == USER_STATE::NONE) {
		//std::cout << "handler_interface->GetManagerInterface()->Disconnect(id);\n";
		return;
	}

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
		RoutePacket(p_buffer, user_index);

		 // 처리한 패킷은 남은 데이터에서 제거
		users[user_index]->SetRemainDataSize(-packet_size);
		memmove(users[user_index]->GetExOver().packet_buf, users[user_index]->GetExOver().packet_buf + packet_size, users[user_index]->GetRemainDataSize());
		if (users[user_index]->GetRemainDataSize() <= sizeof(short)) break;
		packet_size = users[user_index]->GetPacketSize(users[user_index]->GetExOver().packet_buf);
	}
}

void IOCPServer::RoutePacket(char* packet, int user_index)
{
	switch (users[user_index]->GetState()) {
	case USER_STATE::NONE:
		return;
	case USER_STATE::LOGIN:
		handler.HandlePacket(packet, user_index);
		break;
	case USER_STATE::LOBBY:
		handler.HandlePacket(packet, user_index);
		break;
	case USER_STATE::ROOM:
		rooms[users[user_index]->GetRoomIndex()]->GetRoomPacketHandler().HandlePacket(packet, user_index);
		break;
	}

}

void IOCPServer::BroadCastLobby(char* packet)
{
	for (auto& user : users) {
		if (user->GetState() == USER_STATE::LOBBY) {// 현재 이 상태는 서버에 연결되어 있는 상태이므로, 나중에 로비, 방에 따라 구분 필요
			user->SendPacket(packet, iocp_handle);
		}
	}
}

void IOCPServer::SendToSelf(char* packet, int self_index)
{
	users[self_index]->SendPacket(packet, iocp_handle);
}

void IOCPServer::CreateRoom(char* packet, int user_index)
{
	int room_index = GetEmptyRoomIndex();
	if (room_index == -1) {
		return;
	}

	users[user_index]->SetRoomIndex(room_index);
	rooms[room_index]->InitRoom(packet, users[user_index]); // 초기화는 그냥 방 내부에서 처리하기.
}

int IOCPServer::GetNewUserId()
{
	return id_generator.fetch_add(1) + 1; // fetch_add는 값을 실제로 원자적으로 증가시키지만, 반환하는 것은 증가 이전의 값
}

int IOCPServer::GetEmptyUserIndex()
{
	for (int i = 0; i < MAX_USER; ++i) {
		if (users[i]->GetState() == USER_STATE::NONE) {
			if (users[i]->SetState(USER_STATE::NONE, USER_STATE::LOGIN)) {
				return i;
			}
		}
	}

	return -1;
}

int IOCPServer::GetEmptyRoomIndex()
{
	for (int i = 0; i < MAX_ROOM; ++i) {
		if (rooms[i]->GetRoomState() == ROOM_STATE::EMPTY) {
			rooms[i]->SetRoomState(ROOM_STATE::WAIT);
			return i;
		}
	}

	return -1;
}

void IOCPServer::Disconnect(int user_index)
{
	if (users[user_index]->GetState() == USER_STATE::NONE) return; // 이미 끊김->또 send -> send 실패 -> PQCS -> Disconnect 무한루프 방지

	if (users[user_index]->GetState() == USER_STATE::ROOM) rooms[users[user_index]->GetRoomIndex()]->DeleteUser(users[user_index]->GetId());
	
	S2C_DISCONNECT_PACKET p;
	p.size = sizeof(S2C_DISCONNECT_PACKET);
	p.type = S2C_DISCONNECT;
	//SendToSelf(reinterpret_cast<char*>(&p), user_index);
	closesocket(users[user_index]->GetSocket()); // closesocket 이후 이전 소켓에 대한 iocp 완료(실패로) 통지가 언제 올지 불분명해서 다음에 연결된 소켓이 받을 경우 영향이 갈 수 있다고 하는데..
	users[user_index]->SetState(USER_STATE::NONE);
	std::cout << "Session[" << user_index << "] disconnect/Id: " << id_generator << std::endl;
}

void IOCPServer::ProcessDB(DBOverlapped* db_over, int user_index)
{
	switch (db_over->type) {
	case DBOperationType::LOGIN: {
		S2C_LOGIN_PACKET login_p;
		ZeroMemory(&login_p, sizeof(login_p));
		login_p.size = sizeof(S2C_LOGIN_PACKET);
		login_p.type = S2C_LOGIN;
		if (db_over->ok) {
			if (db_over->result_data) { // nullptr이 아니면, 즉 포인터가 존재하면
				users[user_index]->InitDBInfo(static_cast<DBResultLogin*>(db_over->result_data.get()));
				users[user_index]->SetState(USER_STATE::LOBBY);
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
	case DBOperationType::SCORE_UPDATE:
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

