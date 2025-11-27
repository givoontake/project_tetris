#include "RoomSession.h"

RoomSession::RoomSession()
{

}

RoomSession::~RoomSession()
{

}

void RoomSession::SetIsReady()
{
	is_ready = !is_ready;
}

bool RoomSession::SetUse(bool expected, bool desired)
{
	// false -> true
	if (desired == true) {
		if (in_use.Compare_exchange_strong(expected, desired)) return true;
		else return false;
	}

	// true -> false
	else {
		if (in_use.Compare_exchange_strong(expected, desired)) return true;
		else return false;
	}
}

void RoomSession::InitSession(Session* s)
{
	session = s;
	is_ready = false;
	in_use = true;
	tetris.ClearBoard();
}

void RoomSession::ClearSession()
{
	session = nullptr;
	is_ready = false;
	in_use = false;
	tetris.ClearBoard();
}

