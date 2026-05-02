#pragma once
#include <mutex>
#include "Types.h"
#include "Session.h"
#include "stress_test_files/MetricsPacket.h"

class ViewSession
{
	SP<Session> session;
	int id = VIEW_SESSION_ID;
	bool is_active = false;
	mutable std::mutex view_mutex;

public:
	ViewSession();
	~ViewSession();

	void InitSession(const SP<Session>& new_session);
	void ClearSession();
	bool SendPacket(char* packet, int packet_size, const HANDLE iocp_handle);

	int GetId() const { return id; }
	bool IsActive() const;
};

