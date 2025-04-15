#include <iostream>
#include "IOCPServer.h"

IOCPServer::IOCPServer()
{
	WSAStartup(MAKEWORD(2, 2), &wsadata);

	listen_socket = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);
	client_socket = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);
	memset(&server_addr, 0, sizeof(server_addr));
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(PORT_NUM);
	server_addr.sin_addr.S_un.S_addr = INADDR_ANY;

	accept_over.SetExOverlapped(ACCEPT);

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
				std::cout << "client[" << key << "]" << " Disconnect\n";
				//disconnect(static_cast<int>(key));
				if (ex_over->op_type == SEND) delete ex_over;
				continue;
			}
		}

		// 클라이언트 정상 종료일 경우
		if (transferred_bytes == 0 && ex_over->op_type != ACCEPT) {
			std::cout << "client[" << key << "]" << " Disconnect\n";
			//disconnect(static_cast<int>(key));
			if (ex_over->op_type == SEND) delete ex_over;
			continue;
		}

		switch (ex_over->op_type)
		{
		case ACCEPT: {
			int new_id = GetUserId();
			if (new_id != -1) {
				users[new_id].SetId(new_id);
				users[new_id].SetSocket(client_socket);
				CreateIoCompletionPort(reinterpret_cast<HANDLE>(client_socket), iocp_handle, new_id, 0);
				users[new_id].RecvPacket();
				client_socket = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);
			}
			else std::cout << "서버가 혼잡합니다. 연결을 종료합니다.\n";

			ZeroMemory(&accept_over.over, sizeof(accept_over.over));
			int addr_size = sizeof(SOCKADDR_IN);
			AcceptEx(listen_socket, client_socket, accept_over.packet_buf, 0, addr_size + 16, addr_size + 16, 0, &accept_over.over);
			break;
		}

		case RECV: {
				
			break;
		}

		case SEND:
			// 송신 완료 후 추가 처리
			break;
		}
	}
}

int IOCPServer::GetUserId()
{
	for (int i = 0; i < MAX_USER; ++i) {
		bool expected = false;
		if (id_container[i].compare_exchange_strong(expected, true)) {
			return i;
		}
	}

	return -1;
}

void IOCPServer::Disconnect(int user_id)
{
	// 채팅에 연결된 모든 클라에게 disconnect 패킷 전송-> 실시간 채팅도 아니고 필요 없을 듯 한데..
	id_container[user_id] = false;
	closesocket(users[user_id].GetSocket());
}
