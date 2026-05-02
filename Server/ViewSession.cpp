#include "ViewSession.h"

ViewSession::ViewSession()
{
}

ViewSession::~ViewSession()
{
	ClearSession();
}

void ViewSession::InitSession(const SP<Session>& new_session)
{
	std::lock_guard<std::mutex> lock(view_mutex);
	session = new_session;
	id = VIEW_SESSION_ID;
	is_active = session != nullptr;
}

void ViewSession::ClearSession()
{
	std::lock_guard<std::mutex> lock(view_mutex);
	session.reset();
	is_active = false;
	id = VIEW_SESSION_ID;
}

bool ViewSession::SendPacket(char* packet, int packet_size, const HANDLE iocp_handle)
{
	SP<Session> session_ptr;
	{
		std::lock_guard<std::mutex> lock(view_mutex);
		if (!is_active || !session) return false;
		session_ptr = session;
	}

	if (session_ptr->GetLifeState() != LIFE_STATE::ACTIVE) {
		ClearSession();
		return false;
	}

	session_ptr->SendBoundPacket(packet, packet_size, iocp_handle);
	return true;
}

bool ViewSession::IsActive() const
{
	std::lock_guard<std::mutex> lock(view_mutex);
	return is_active && session && session->GetLifeState() == LIFE_STATE::ACTIVE;
}
