#include <iostream>
#include "IOThread.h"
#include "IOCPServer.h"

IOThread::IOThread(IOCPServer& iocp_server)
	: iocp_server_(iocp_server)
{
}

void IOThread::Run()
{
	while (is_running_.load() && iocp_server_.is_running_.load()) {
		DWORD transferred_bytes = 0;
		ULONG_PTR completion_key = 0;
		WSAOVERLAPPED* over = nullptr;
		BOOL is_result = GetQueuedCompletionStatus(iocp_server_.iocp_handle_, &transferred_bytes, &completion_key, &over, INFINITE); // 인자로 넘긴 주소 변수의 값을 채워준다.
		if (over == nullptr) continue;

		ExOverlapped* ex_over = reinterpret_cast<ExOverlapped*>(over);
		switch (completion_key) {
		case LISTEN_IO_COMPLETION: {
			if (ex_over->op_type != OPType::ACCEPT) break;
			if (!is_result) {
				std::cout << "Accept Error" << WSAGetLastError() << "\n";
				break;
			}
			int new_index = iocp_server_.GetEmptyUserIndex();
			if (new_index != -1) {
				auto new_session = std::make_shared<Session>();
				new_session->SetIndex(new_index);
				new_session->InitSession(iocp_server_.client_socket_);
				SP<Session> expected = nullptr;
				if (iocp_server_.users_[new_index].compare_exchange_strong(expected, new_session)) {
					CreateIoCompletionPort(reinterpret_cast<HANDLE>(iocp_server_.client_socket_), iocp_server_.iocp_handle_, SESSION_IO_COMPLETION, 0);
					new_session->RecvPacket(iocp_server_.iocp_handle_);
					iocp_server_.client_socket_ = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);
					std::cout << "Session[" << new_index << "] connect" << std::endl;
				}
				else {
					closesocket(iocp_server_.client_socket_);
					iocp_server_.client_socket_ = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);
				}
			}

			else {
				closesocket(iocp_server_.client_socket_);
				iocp_server_.client_socket_ = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);
			}

			if (iocp_server_.client_socket_ != INVALID_SOCKET) {
				ZeroMemory(&iocp_server_.accept_over_.ex_over.over, sizeof(iocp_server_.accept_over_.ex_over.over));
				int addr_size = sizeof(SOCKADDR_IN);
				bool is_res = AcceptEx(iocp_server_.listen_socket_, iocp_server_.client_socket_, iocp_server_.accept_over_.packet_buf, 0, addr_size + 16, addr_size + 16, 0, &iocp_server_.accept_over_.ex_over.over);
				if (!is_res && WSAGetLastError() != ERROR_IO_PENDING) std::cerr << "AcceptEx fail.. " << std::endl;
			}
			break;
		}

		case SESSION_IO_COMPLETION: {
			IOOverlapped* io_over = reinterpret_cast<IOOverlapped*>(ex_over);
			auto sess_ptr = iocp_server_.FindSessionByIndex(ex_over->key.index);
			if (!sess_ptr) {
				if (ex_over->op_type == OPType::SEND) delete io_over;
				break;
			}
			Session& sess = *sess_ptr;
			switch (ex_over->op_type) {
			case OPType::RECV: {
				if (!is_result || transferred_bytes == 0) {
					if (sess.BeginDeactivate()) {
						iocp_server_.BeginDisconnect(sess_ptr); // 팬딩이 0으로 노출되면 다른 곳에서 disconnect 관련 작업이 일어날 수 있다.
					}
					sess.ReducePending();
					if (sess.TryDeactivate()) iocp_server_.TryDisconnect(sess_ptr);

					break;
				}
				else {
					iocp_server_.ProcessPacket(sess_ptr, transferred_bytes); // recv 토큰은 정상 수신 중 유지하고 disconnect 경로에서만 줄인다.
					sess.ReducePending();
					if (sess.TryDeactivate()) iocp_server_.TryDisconnect(sess_ptr);
					sess.RecvPacket(iocp_server_.iocp_handle_);
				}
				break;
			}

			case OPType::SEND: {
				if ((!is_result || transferred_bytes == 0)) {
					if (sess.BeginDeactivate()) {
						iocp_server_.BeginDisconnect(sess_ptr);
					}
					sess.ReducePending();
					if (sess.TryDeactivate()) iocp_server_.TryDisconnect(sess_ptr);
				}
				else {
					sess.ReducePending();
					if (sess.TryDeactivate()) iocp_server_.TryDisconnect(sess_ptr);
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
			if (ex_over->op_type == OPType::DELETE_ROOM) iocp_server_.DeleteRoom(ex_over->room_index);
			delete ex_over;
			break;

		case DB_SESSION_COMPLETION: { // 팬딩은 모든 작업을 마치고 줄야야 함
			DBOverlapped* db_over = reinterpret_cast<DBOverlapped*>(ex_over);
			int index = db_over->ex_over.key.index;
			auto sess_ptr = iocp_server_.FindSessionByIndex(index);
			if (!sess_ptr) {
				delete db_over;
				break;
			}
			if (!is_result) {
				if (sess_ptr->BeginDeactivate()) {
					iocp_server_.BeginDisconnect(sess_ptr);
				}
			}
			else {
				iocp_server_.db_result_handler_.HandleIOResult(db_over, sess_ptr);
			}
			sess_ptr->ReducePending();
			if (sess_ptr->TryDeactivate()) iocp_server_.TryDisconnect(sess_ptr);
			delete db_over;
			break;
		}

		case DB_SERVER_COMPLETION: {
			if (!is_result) {
				iocp_server_.is_running_.store(false);
			}
			DBOverlapped* db_over = reinterpret_cast<DBOverlapped*>(ex_over);
			iocp_server_.db_result_handler_.HandleInitServerResult(db_over);
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
	is_running_ = false;
	PostQueuedCompletionStatus(iocp_server_.iocp_handle_, 0, 0, nullptr);
}
