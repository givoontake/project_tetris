#include "RoomSession.h"

RoomSession::RoomSession(C2S_ADD_USER_PACKET& p)
{
	memcpy(user_name, p.name, sizeof(user_name));
	user_id = p.id;
	user_state = WAIT;
}

RoomSession::~RoomSession()
{

}
