#include <iostream>
#include "IOCPServer.h"

IOCPServer::IOCPServer()
{
	packet_handler = std::make_unique<PacketHandler>(this);
	for (auto& user : users) {
		user = std::make_unique<Session>(packet_handler.get()); // packet_handler는 unique_ptr이므로 get()을 이용해 raw ptr을 넘긴다.
	}

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

std::array<std::unique_ptr<Session>, MAX_USER>& IOCPServer::GetSessionList()
{
	return users;
}

MQueue& IOCPServer::GetQueue()
{
	return disconnect_queue;
	// TODO: 여기에 return 문을 삽입합니다.
}

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

		ExOvelapped* ex_over = reinterpret_cast<ExOvelapped*>(over);

		if (!result){
			if (ex_over->op_type == ACCEPT) std::cout << "Accept Error";
			else { // 클라이언트 강제 종료일 경우
				Disconnect(static_cast<int>(key));
				if (ex_over->op_type == SEND) delete ex_over;
				continue;
			}
		}

		// 클라이언트 정상 종료일 경우
		if (transferred_bytes == 0 && ex_over->op_type != ACCEPT) {
			Disconnect(static_cast<int>(key));
			if (ex_over->op_type == SEND) delete ex_over;
			continue;
		}

		switch (ex_over->op_type)
		{
		case ACCEPT: {
			int new_id = GetUserId();
			if (new_id != -1) {
				users[new_id]->SetId(new_id);
				users[new_id]->SetSocket(client_socket);
				CreateIoCompletionPort(reinterpret_cast<HANDLE>(client_socket), iocp_handle, new_id, 0);
				users[new_id]->RecvPacket();
				client_socket = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);
			}
			else std::cout << "서버가 혼잡합니다. 연결을 종료합니다.\n";

			std::cout << "client[" << new_id << "]" << " Connect\n";

			ZeroMemory(&accept_over.over, sizeof(accept_over.over));
			int addr_size = sizeof(SOCKADDR_IN);
			AcceptEx(listen_socket, client_socket, accept_over.packet_buf, 0, addr_size + 16, addr_size + 16, 0, &accept_over.over);
			break;
		}

		case RECV: {
			users[key]->MergePacket(transferred_bytes, ex_over->packet_buf);
			users[key]->RecvPacket();
			break;
		}

		case SEND:
			delete ex_over;
			// 송신 완료 후 추가 처리
			break;
		}
	}
}

int IOCPServer::GetUserId()
{
	for (int i = 0; i < MAX_USER; ++i) {
		if (!users[i]->GetUse()) {
			if (users[i]->SetUse(false, true)) {
				return i;
			}
		}
	}

	return -1;
}

void IOCPServer::Disconnect(int user_id)
{
	std::cout << "client[" << user_id << "]" << " Disconnect\n";
	users[user_id]->SetUse(true, false); // 원래는 카스는 필요 없긴 한데.. 함수를 또 만드는게 번거로워서 그냥 하나에 만들었다.
	closesocket(users[user_id]->GetSocket());
	S2C_DISCONNECT_PACKET p;
	p.size = sizeof(S2C_DISCONNECT_PACKET);
	p.type = S2C_DISCONNECT;
	for (auto& user : users){
		if (!user->GetUse() || user_id == user->GetId()) continue;
		user->SendPacket(reinterpret_cast<char*>(&p));
	}
}
