#include <iostream>
#include "IOThread.h"
#include "../IOCPServer.h"

IOThread::IOThread(IOCPServer& iocp_server)
	: iocp_server(iocp_server)
{
}

void IOThread::Run()
{
	while (running.load() && iocp_server.is_running.load()) {
		DWORD transferred_bytes = 0;
		ULONG_PTR completion_key = 0;
		WSAOVERLAPPED* over = nullptr;
		BOOL result = GetQueuedCompletionStatus(iocp_server.iocp_handle, &transferred_bytes, &completion_key, &over, INFINITE); // 인자로 넘긴 주소 변수의 값을 채워준다.
		if (over == nullptr) continue;

		ExOverlapped* ex_over = reinterpret_cast<ExOverlapped*>(over);
		switch (completion_key) {
		case LISTEN_IO_COMPLETION: {
			if (ex_over->op_type != OP_TYPE::ACCEPT) break;
			if (!result) {
				std::cout << "Accept Error" << WSAGetLastError() << "\n";
				break;
			}
			int new_index = iocp_server.GetEmptyUserIndex();
			if (new_index != -1) {
				auto new_session = std::make_shared<Session>();
				new_session->SetIndex(new_index);
				new_session->InitSession(iocp_server.client_socket);
				SP<Session> expected = nullptr;
				if (iocp_server.users[new_index].compare_exchange_strong(expected, new_session)) {
					CreateIoCompletionPort(reinterpret_cast<HANDLE>(iocp_server.client_socket), iocp_server.iocp_handle, SESSION_IO_COMPLETION, 0);
					new_session->RecvPacket(iocp_server.iocp_handle);
					iocp_server.client_socket = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);
					std::cout << "Session[" << new_index << "] connect" << std::endl;
				}
				else {
					closesocket(iocp_server.client_socket);
					iocp_server.client_socket = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);
				}
			}

			else {
				closesocket(iocp_server.client_socket);
				iocp_server.client_socket = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);
			}

			if (iocp_server.client_socket != INVALID_SOCKET) {
				ZeroMemory(&iocp_server.accept_over.ex_over.over, sizeof(iocp_server.accept_over.ex_over.over));
				int addr_size = sizeof(SOCKADDR_IN);
				bool res = AcceptEx(iocp_server.listen_socket, iocp_server.client_socket, iocp_server.accept_over.packet_buf, 0, addr_size + 16, addr_size + 16, 0, &iocp_server.accept_over.ex_over.over);
				if (!res && WSAGetLastError() != ERROR_IO_PENDING) std::cerr << "AcceptEx fail.. " << std::endl;
			}
			break;
		}

		case SESSION_IO_COMPLETION: {
			IOOverlapped* io_over = reinterpret_cast<IOOverlapped*>(ex_over);
			auto sess_ptr = iocp_server.FindSessionByIndex(ex_over->key.index);
			if (!sess_ptr) {
				if (ex_over->op_type == OP_TYPE::SEND) delete io_over;
				break;
			}
			Session& sess = *sess_ptr;
			switch (ex_over->op_type) {
			case OP_TYPE::RECV: {
				if (!result || transferred_bytes == 0) {
					if (sess.BeginDeactivate()) {
						iocp_server.BeginDisconnect(sess_ptr); // 팬딩이 0으로 노출되면 다른 곳에서 disconnect 관련 작업이 일어날 수 있다.
					}
					sess.ReducePending();
					if (sess.TryDeactivate()) iocp_server.TryDisconnect(sess_ptr);

					break;
				}
				else {
					iocp_server.ProcessPacket(sess_ptr, transferred_bytes); // recv 토큰은 정상 수신 중 유지하고 disconnect 경로에서만 줄인다.
					sess.ReducePending();
					if (sess.TryDeactivate()) iocp_server.TryDisconnect(sess_ptr);
					sess.RecvPacket(iocp_server.iocp_handle);
				}
				break;
			}

			case OP_TYPE::SEND: {
				if ((!result || transferred_bytes == 0)) {
					if (sess.BeginDeactivate()) {
						iocp_server.BeginDisconnect(sess_ptr);
					}
					sess.ReducePending();
					if (sess.TryDeactivate()) iocp_server.TryDisconnect(sess_ptr);
				}
				else {
					sess.ReducePending();
					if (sess.TryDeactivate()) iocp_server.TryDisconnect(sess_ptr);
				}
				
				delete io_over;
				break;
			}

			default:
				break;
			}
			break;
		}

		case ROOM_IO_COMPLETION:
			if (ex_over->op_type == OP_TYPE::DELETE_ROOM) iocp_server.DeleteRoom(ex_over->room_index);
			delete ex_over;
			break;

		case DB_SESSION_COMPLETION: { // 팬딩은 모든 작업을 마치고 줄야야 함
			DBOverlapped* db_over = reinterpret_cast<DBOverlapped*>(ex_over);
			int index = db_over->ex_over.key.index;
			auto sess_ptr = iocp_server.FindSessionByIndex(index);
			if (!sess_ptr) {
				delete db_over;
				break;
			}
			if (!result) {
				if (sess_ptr->BeginDeactivate()) {
					iocp_server.BeginDisconnect(sess_ptr);
				}
			}
			else {
				iocp_server.db_result_handler.HandleIOResult(db_over, sess_ptr);
			}
			sess_ptr->ReducePending();
			if (sess_ptr->TryDeactivate()) iocp_server.TryDisconnect(sess_ptr);
			delete db_over;
			break;
		}

		case DB_SERVER_COMPLETION: {
			if (!result) {
				iocp_server.is_running.store(false);
			}
			DBOverlapped* db_over = reinterpret_cast<DBOverlapped*>(ex_over);
			iocp_server.db_result_handler.HandleInitServerResult(db_over);
			delete db_over;
			break;
		}
		
		default:
			break;
		}
	}
}

void IOThread::Close()
{
	running = false;
	PostQueuedCompletionStatus(iocp_server.iocp_handle, 0, 0, nullptr);
}
