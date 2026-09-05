#include <iostream>
#include "IOThread.h"
#include "TetrisServer.h"
#include "session_tasks.h"

IOThread::IOThread(TetrisServer& tetris_server)
	: tetris_server_(tetris_server)
{
}

void IOThread::Run()
{
	while (is_running_.load() && tetris_server_.IsRunning()) {
		DWORD transferred_bytes = 0;
		ULONG_PTR completion_key = 0;
		WSAOVERLAPPED* over = nullptr;
		BOOL result = GetQueuedCompletionStatus(tetris_server_.GetIOCPHandle(), &transferred_bytes, &completion_key, &over, INFINITE);
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

	tetris_server_.CompleteAccept();
}

void IOThread::ProcessSessionCompletion(BOOL result, DWORD transferred_bytes, ExOverlapped* ex_over)
{
	const bool is_send = ex_over->op_type == OPType::SEND || ex_over->op_type == OPType::POOLED_SEND;
	IOOverlapped* io_over = reinterpret_cast<IOOverlapped*>(ex_over);
	SendBuffer* send_buffer = is_send ? static_cast<SendBuffer*>(io_over) : nullptr;

	switch (ex_over->op_type) {
	case OPType::RECV: {
		auto* session = tetris_server_.FindSession(ex_over->session_key);
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
		tetris_server_.RequestDisconnect(session_key);
		tetris_server_.CompleteSessionIO(session_key);
		return;
	}

	const bool should_receive = tetris_server_.ProcessRecvBuffer(session, session_key, transferred_bytes);
	if (should_receive) session.RecvPacket(tetris_server_.GetIOCPHandle());
	tetris_server_.CompleteSessionIO(session_key);
}

void IOThread::ProcessSendCompletion(BOOL result, DWORD transferred_bytes, SendBuffer* send_buffer, SessionKey session_key)
{
	if (!result || transferred_bytes == 0) tetris_server_.RequestDisconnect(session_key);

	if (send_buffer->ex_over.op_type == OPType::POOLED_SEND) send_buffer->Clear();
	else delete send_buffer;
	tetris_server_.CompleteSessionIO(session_key);
}

void IOThread::ProcessSessionDBCompletion(BOOL result, ExOverlapped* ex_over)
{
	std::unique_ptr<DBOverlapped> db_over(reinterpret_cast<DBOverlapped*>(ex_over));
	const SessionKey session_key = db_over->ex_over.session_key;
	if (!result) {
		tetris_server_.RequestDisconnect(session_key);
	}
	else {
		tetris_server_.EnqueueSessionTask(session_key, std::make_unique<SessionDBResultTask>(std::move(db_over)));
		return;
	}
}

void IOThread::ProcessServerDBCompletion(BOOL result, ExOverlapped* ex_over)
{
	if (!result) {
		tetris_server_.RequestStop();
	}
	DBOverlapped* db_over = reinterpret_cast<DBOverlapped*>(ex_over);
	tetris_server_.GetDBResultHandler().HandleServerDBResult(db_over);
	delete db_over;
}

void IOThread::Close()
{
	is_running_ = false;
	PostQueuedCompletionStatus(tetris_server_.GetIOCPHandle(), 0, 0, nullptr);
}
