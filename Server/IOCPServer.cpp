#include <iostream>
#include "IOCPServer.h"

IOCPServer::IOCPServer() : handler(this)
{
	for (int i = 0; i < MAX_USER; ++i) {
		users[i] = new Session();
	}

	for (int i = 0; i < MAX_ROOM; ++i){
		rooms[i] = new TetrisRoom();
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

	accept_over.SetExOverlapped(ACCEPT);

}

IOCPServer::~IOCPServer()
{
	closesocket(listen_socket);
	closesocket(client_socket);
	WSACleanup();
}

//std::array<std::unique_ptr<Session>, MAX_USER>& IOCPServer::GetSessionList()
//{
//	return users;
//}

//MQueue& IOCPServer::GetTaskQueue()
//{
//	return task_queue;
//}

void IOCPServer::StartServer()
{
	bind(listen_socket, reinterpret_cast<sockaddr*>(&server_addr), sizeof(server_addr));
	listen(listen_socket, SOMAXCONN);
	iocp_handle = CreateIoCompletionPort(INVALID_HANDLE_VALUE, 0, 0, 0);
	CreateIoCompletionPort(reinterpret_cast<HANDLE>(listen_socket), iocp_handle, 9999, 0);
	int addr_size = sizeof(SOCKADDR_IN);
	AcceptEx(listen_socket, client_socket, accept_over.packet_buf, 0, addr_size + 16, addr_size + 16, 0, &accept_over.over);
}

void IOCPServer::ProcessGQCS()
{
	while (is_running)
	{
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
			if (ex_over->op_type == ACCEPT) std::cout << "Accept Error" << WSAGetLastError() << "\n";
			else { // 클라이언트 강제 종료일 경우
				Disconnect(static_cast<int>(key));
				if (ex_over->op_type == SEND) delete ex_over;
			}
			continue;
		}

		// 클라이언트 정상 종료일 경우
		if (transferred_bytes == 0 && ex_over->op_type != ACCEPT) {
			Disconnect(static_cast<int>(key));
			if (ex_over->op_type == SEND) delete ex_over;
			continue;
		}

		switch (ex_over->op_type){
		case ACCEPT: { 
			int new_id = GetUserId();
			if (new_id != -1) {
				users[new_id]->InitSession(new_id, client_socket);
				CreateIoCompletionPort(reinterpret_cast<HANDLE>(client_socket), iocp_handle, new_id, 0);
				users[new_id]->RecvPacket();
				client_socket = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);
			}
			else std::cout << "서버가 혼잡합니다. 연결을 종료합니다.\n";

			++user_count;
			std::cout << "client[" << new_id << "]" << " Connect. " << "total_user: " << user_count << std::endl;
			ZeroMemory(&accept_over.over, sizeof(accept_over.over));
			int addr_size = sizeof(SOCKADDR_IN);
			AcceptEx(listen_socket, client_socket, accept_over.packet_buf, 0, addr_size + 16, addr_size + 16, 0, &accept_over.over);
			break;
		}

		case RECV: {
			ProcessPacket(transferred_bytes, key);
			--remainning_total_IOCP;
			users[key]->RecvPacket();
			break;
		}

		case SEND: {
			delete ex_over;
			--remainning_send_IOCP;
			--remainning_total_IOCP;
			//if ((remainning_send_IOCP > 100) && (remainning_send_IOCP % 100 == 0)) std::cout << "remainning_send_IOCP: " << remainning_send_IOCP << std::endl;
			// 송신 완료 후 추가 처리
			break;
		}
		
		}
		++processed_IOCP;
		if(processed_IOCP % 1000 == 0) std::cout << "r_send: " << remainning_send_IOCP <<" r_total: " << remainning_total_IOCP << " processed: " << processed_IOCP << std::endl;
	}
}

void IOCPServer::ProcessPacket(int recv_bytes, int user_id)
{
	if (users[user_id]->GetState() == false) {
		//std::cout << "handler_interface->GetManagerInterface()->Disconnect(id);\n";
		return;
	}

	// 모든 패킷 처리는 중앙 서버에서 하도록 변경할 예정
	// 중앙 서버의 디스커넥트에 직접 접근하도록 하지 말고, 디스커넥트 요청이 온 것처럼(예전에 생각한 방식)처리.. 아니다. 굳이 번거롭게 이러지 말자
	// 그냥 disconnect 처리 여부를 담당하는 플래그를 하나 만들자. 서버에서 패킷을 처리할 때 마다 이 플래그를 검사하도록 하자.
	// 그러면 간편해진다.
	if (recv_bytes + users[user_id]->GetRemainDataSize() > BUF_SIZE) {
		//handler_interface->GetServerInterface()->Disconnect(id);
		users[user_id]->SetDisconnectFlag(true);
		return;
	}

	else users[user_id]->SetRemainDataSize(recv_bytes);

	if (users[user_id]->GetRemainDataSize() < sizeof(short)) return;

	short packet_size = users[user_id]->GetPacketSize(users[user_id]->GetExOver().packet_buf);

	// ---구조를 변경하면서 고민되는 점---
	// 단순히 세션이 처리 가능한 데이터가 있다고 작업 큐에 넣느냐-> 나중에 서버단에서 처리 후 데이터 땡기기 등 처리를 해야함
	// 세션에 작업 가능한 데이터를 잘라 따로 보관해 놓는다-> 나중에 서버가 따로 신경 쓸 부분은 없어짐.. 그러나 추가 메모리 필요
	// 뭐가 나으려나??
	while (users[user_id]->GetRemainDataSize() >= packet_size) // 남아있는 데이터 크기가 실제 처리가능한 데이터 크기이상 존재한다면
	{
		packet_size = users[user_id]->GetPacketSize(users[user_id]->GetExOver().packet_buf);
		char p_buffer[BUF_SIZE];
		// 패킷 분리: packet_buffer에 복사 후 처리
		memcpy(p_buffer, users[user_id]->GetExOver().packet_buf, packet_size);
		handler.HandlePacket(p_buffer);

		 // 처리한 패킷은 남은 데이터에서 제거
		users[user_id]->SetRemainDataSize(-packet_size);
		memmove(users[user_id]->GetExOver().packet_buf, users[user_id]->GetExOver().packet_buf + packet_size, users[user_id]->GetRemainDataSize());
		//handler_interface->HandlePacket(p_buffer);
	}
}

void IOCPServer::BroadCastLobby(char* packet)
{
	for (auto& user : users) {
		if (user->GetState() == LOBBY) {// 현재 이 상태는 서버에 연결되어 있는 상태이므로, 나중에 로비, 방에 따라 구분 필요
			user->SendPacket(packet);
		}
	}
}

void IOCPServer::SendToSelf(char* packet, int self_id)
{
	users[self_id]->SendPacket(packet);
}

void IOCPServer::CreateRoom(char* packet)
{
	int room_id = GetRoomId();
	if (room_id == -1) {
		return;
	}

	// 어차피 오픈과 록은 마지막에 비밀번호 필드 차이 유무이므로, 그냥 오픈 구조체로 만들고 id에 접근한다.
	C2S_ADD_OPEN_ROOM_PACKET* p = reinterpret_cast<C2S_ADD_OPEN_ROOM_PACKET*>(packet);
	users[p->id]->SetRoomId(room_id); // 방에서 가진 룸 세션은 세션을 가지지 않으므로 접근해서 room_id 세팅이 불가능함
	rooms[room_id]->InitRoom(packet); // 초기화는 그냥 방 내부에서 처리하기.
}

int IOCPServer::GetUserId()
{
	for (int i = 0; i < MAX_USER; ++i) {
		if (!users[i]->GetState()) {
			if (users[i]->SetUse(NONE, LOBBY)) {
				return i;
			}
		}
	}

	return -1;
}

int IOCPServer::GetRoomId()
{
	for (int i = 0; i < MAX_ROOM; ++i) {
		if (rooms[i]->GetRoomState() == EMPTY) {
			rooms[i]->SetRoomState(WAIT);
			return i;
		}
	}

	return -1;
}

void IOCPServer::Disconnect(int user_id)
{
	users[user_id]->SetUse(NONE);
	closesocket(users[user_id]->GetSocket());
	std::cout << "client[" << user_id << "]" << " Disconnect\n";
	--user_count;
	std::cout << "client[" << user_id << "]" << " Connect. " << "total_user: " << user_count << std::endl;
	// 디스커넥트 처리 패킷을 왜 다른애들이 알아야 하지.. 게임에 참가한 상태에서만 남들이 알면 그만이다. 애초에 본인한테 전송할 이유도 없으니 그냥 처리만 하면 된다.
	// 나중에 게임 방에서는 브로드캐스트 필요.
	
	//S2C_DISCONNECT_PACKET p;
	//p.size = sizeof(S2C_DISCONNECT_PACKET);
	//p.type = S2C_DISCONNECT;
	//for (auto& user : users){
	//	if (!user->GetUse() || user_id == user->GetId()) continue;
	//	user->SendPacket(reinterpret_cast<char*>(&p));
	//}
}
