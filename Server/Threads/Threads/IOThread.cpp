#include <iostream>
#include "IOThread.h"
#include "IOCPServer.h"
#include "session_tasks.h"

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

	auto* new_session = iocp_server_.AcquireSession(iocp_server_.accept_socket_);
	if (new_session) {
		CreateIoCompletionPort(reinterpret_cast<HANDLE>(iocp_server_.accept_socket_), iocp_server_.iocp_handle_, SESSION_IO_COMPLETION, 0);
		new_session->RecvPacket(iocp_server_.iocp_handle_);
		std::cout << "Session[" << new_session->GetSessionKey().session_index << "] connect" << std::endl;
	}
	else {
		closesocket(iocp_server_.accept_socket_);
	}
	iocp_server_.accept_socket_ = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);

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

	switch (ex_over->op_type) {
	case OPType::RECV: {
		auto* session = iocp_server_.FindSession(ex_over->session_key);
		if (!session) return;
		ProcessReceiveCompletion(result, transferred_bytes, *session, ex_over->session_key);
		break;
	}

	case OPType::SEND:
	case OPType::POOLED_SEND:
		ProcessSendCompletion(result, transferred_bytes, send_buffer, ex_over->session_key);
		break;

	default:
		break;
	}
}

void IOThread::ProcessReceiveCompletion(BOOL result, DWORD transferred_bytes, Session& session, SessionKey session_key)
{
	if (!result || transferred_bytes == 0) {
		iocp_server_.RequestDisconnect(session_key);
		iocp_server_.CompleteSessionIO(session_key);
		return;
	}

	const bool should_receive = iocp_server_.ProcessRecvBuffer(session, session_key, transferred_bytes);
	if (should_receive) session.RecvPacket(iocp_server_.iocp_handle_);
	iocp_server_.CompleteSessionIO(session_key);
}

void IOThread::ProcessSendCompletion(BOOL result, DWORD transferred_bytes, SendBuffer* send_buffer, SessionKey session_key)
{
	if (!result || transferred_bytes == 0) iocp_server_.RequestDisconnect(session_key);

	if (send_buffer->ex_over.op_type == OPType::POOLED_SEND) send_buffer->Clear();
	else delete send_buffer;
	iocp_server_.CompleteSessionIO(session_key);
}

void IOThread::ProcessSessionDBCompletion(BOOL result, ExOverlapped* ex_over)
{
	std::unique_ptr<DBOverlapped> db_over(reinterpret_cast<DBOverlapped*>(ex_over));
	const SessionKey session_key = db_over->ex_over.session_key;
	if (!result) {
		iocp_server_.RequestDisconnect(session_key);
	}
	else {
		iocp_server_.EnqueueSessionTask(session_key, std::make_unique<SessionDBResultTask>(std::move(db_over)));
		return;
	}
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
