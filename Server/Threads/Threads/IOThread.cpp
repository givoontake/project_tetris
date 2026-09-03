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
		BOOL result = GetQueuedCompletionStatus(iocp_server_.iocp_handle_, &transferred_bytes, &completion_key, &over, INFINITE); // 인자로 넘긴 주소 변수의 값을 채워준다.
		if (over == nullptr) continue;

		ExOverlapped* ex_over = reinterpret_cast<ExOverlapped*>(over);
		switch (completion_key) {
		case LISTEN_IO_COMPLETION:
			ProcessAcceptCompletion(result, ex_over);
			break;

		case SESSION_IO_COMPLETION:
			ProcessSessionCompletion(result, transferred_bytes, ex_over);
			break;

		case ROOM_IO_COMPLETION:
			ProcessRoomCompletion(ex_over);
			break;

		case DB_SESSION_COMPLETION:
			ProcessSessionDBCompletion(result, ex_over);
			break;

		case DB_SERVER_COMPLETION:
			ProcessServerDBCompletion(result, ex_over);
			break;

		default:
			break;
		}
	}
}

void IOThread::ProcessAcceptCompletion(BOOL result, ExOverlapped* ex_over)
{
	if (ex_over->op_type != OPType::ACCEPT) return;
	if (!result) {
		std::cout << "Accept Error" << WSAGetLastError() << "\n";
		return;
	}

	int new_index = iocp_server_.FindAvailableSessionIndex();
	if (new_index != -1) {
		auto new_session = std::make_shared<Session>();
		new_session->SetSessionIndex(new_index);
		new_session->InitSession(iocp_server_.accept_socket_);
		SP<Session> expected = nullptr;
		if (iocp_server_.sessions_[new_index].compare_exchange_strong(expected, new_session)) {
			CreateIoCompletionPort(reinterpret_cast<HANDLE>(iocp_server_.accept_socket_), iocp_server_.iocp_handle_, SESSION_IO_COMPLETION, 0);
			new_session->RecvPacket(iocp_server_.iocp_handle_);
			iocp_server_.accept_socket_ = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);
			std::cout << "Session[" << new_index << "] connect" << std::endl;
		}
		else {
			closesocket(iocp_server_.accept_socket_);
			iocp_server_.accept_socket_ = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);
		}
	}
	else {
		closesocket(iocp_server_.accept_socket_);
		iocp_server_.accept_socket_ = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);
	}

	if (iocp_server_.accept_socket_ != INVALID_SOCKET) {
		ZeroMemory(&iocp_server_.accept_over_.ex_over.over, sizeof(iocp_server_.accept_over_.ex_over.over));
		int addr_size = sizeof(SOCKADDR_IN);
		result = AcceptEx(iocp_server_.listen_socket_, iocp_server_.accept_socket_, iocp_server_.accept_over_.packet_buffer, 0, addr_size + 16, addr_size + 16, 0, &iocp_server_.accept_over_.ex_over.over);
		if (!result && WSAGetLastError() != ERROR_IO_PENDING) std::cerr << "AcceptEx fail.. " << std::endl;
	}
}

void IOThread::ProcessSessionCompletion(BOOL result, DWORD transferred_bytes, ExOverlapped* ex_over)
{
	const bool is_send = ex_over->op_type == OPType::SEND || ex_over->op_type == OPType::POOLED_SEND;
	IOOverlapped* io_over = reinterpret_cast<IOOverlapped*>(ex_over);
	SendBuffer* send_buffer = is_send ? static_cast<SendBuffer*>(io_over) : nullptr;
	auto session = iocp_server_.FindSessionByIndex(ex_over->session_key.session_index);
	if (!session) {
		if (ex_over->op_type == OPType::POOLED_SEND) send_buffer->Clear();
		else if (ex_over->op_type == OPType::SEND) delete send_buffer;
		return;
	}

	switch (ex_over->op_type) {
	case OPType::RECV:
		ProcessReceiveCompletion(result, transferred_bytes, session);
		break;

	case OPType::SEND:
	case OPType::POOLED_SEND:
		ProcessSendCompletion(result, transferred_bytes, send_buffer, session);
		break;

	default:
		break;
	}
}

void IOThread::ProcessReceiveCompletion(BOOL result, DWORD transferred_bytes, const SP<Session>& session)
{
	if (!result || transferred_bytes == 0) {
		if (session->BeginDeactivate()) {
			iocp_server_.BeginDisconnect(session); // 팬딩이 0으로 노출되면 다른 곳에서 disconnect 관련 작업이 일어날 수 있다.
		}
		session->ReducePending();
		if (session->TryDeactivate()) iocp_server_.TryDisconnect(session);
		return;
	}

	iocp_server_.ProcessRecvBuffer(session, transferred_bytes); // recv 토큰은 정상 수신 중 유지하고 disconnect 경로에서만 줄인다.
	session->ReducePending();
	if (session->TryDeactivate()) iocp_server_.TryDisconnect(session);
	session->RecvPacket(iocp_server_.iocp_handle_);
}

void IOThread::ProcessSendCompletion(BOOL result, DWORD transferred_bytes, SendBuffer* send_buffer, const SP<Session>& session)
{
	if (!result || transferred_bytes == 0) {
		if (session->BeginDeactivate()) {
			iocp_server_.BeginDisconnect(session);
		}
	}
	session->ReducePending();
	if (session->TryDeactivate()) iocp_server_.TryDisconnect(session);

	if (send_buffer->ex_over.op_type == OPType::POOLED_SEND) send_buffer->Clear();
	else delete send_buffer;
}

void IOThread::ProcessRoomCompletion(ExOverlapped* ex_over)
{
	if (ex_over->op_type == OPType::DELETE_ROOM) iocp_server_.DeleteRoom(ex_over->room_index);
	delete ex_over;
}

void IOThread::ProcessSessionDBCompletion(BOOL result, ExOverlapped* ex_over)
{
	// 팬딩은 모든 작업을 마치고 줄야야 함
	DBOverlapped* db_over = reinterpret_cast<DBOverlapped*>(ex_over);
	int index = db_over->ex_over.session_key.session_index;
	auto session = iocp_server_.FindSessionByIndex(index);
	if (!session) {
		delete db_over;
		return;
	}
	if (!result) {
		if (session->BeginDeactivate()) {
			iocp_server_.BeginDisconnect(session);
		}
	}
	else {
		iocp_server_.db_result_handler_.HandleSessionDBResult(db_over, session);
	}
	session->ReducePending();
	if (session->TryDeactivate()) iocp_server_.TryDisconnect(session);
	delete db_over;
}

void IOThread::ProcessServerDBCompletion(BOOL result, ExOverlapped* ex_over)
{
	if (!result) {
		iocp_server_.is_running_.store(false);
	}
	DBOverlapped* db_over = reinterpret_cast<DBOverlapped*>(ex_over);
	iocp_server_.db_result_handler_.HandleServerDBResult(db_over);
	delete db_over;
}

void IOThread::Close()
{
	is_running_ = false;
	PostQueuedCompletionStatus(iocp_server_.iocp_handle_, 0, 0, nullptr);
}
