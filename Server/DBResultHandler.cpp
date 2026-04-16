#include "DBResultHandler.h"
#include "IOCPServer.h"

DBResultHandler::DBResultHandler(IOCPServer& server) : server(server)
{
}

void DBResultHandler::HandleRequestFriendDBResult(DBOverlapped* db_over)
{
	server.HandleRequestFriendDBResult(db_over);
}

void DBResultHandler::HandleAddFriendDBResult(DBOverlapped* db_over)
{
	server.HandleAddFriendDBResult(db_over);
}

void DBResultHandler::HandleDeleteFriendDBResult(DBOverlapped* db_over)
{
	server.HandleDeleteFriendDBResult(db_over);
}

void DBResultHandler::HandleLoginDBResult(DBOverlapped* db_over, Session& session)
{
	server.HandleLoginDBResult(db_over, session);
}

void DBResultHandler::HandleUpdateScoreDBResult(DBOverlapped* db_over, Session& session)
{
	server.HandleUpdateScoreDBResult(db_over, session);
}

void DBResultHandler::HandleUpdateMatchResultDBResult(DBOverlapped* db_over, Session& session)
{
	server.HandleUpdateMatchResultDBResult(db_over, session);
}

void DBResultHandler::HandleLoadFriendListDBResult(DBOverlapped* db_over, Session& session)
{
	server.HandleLoadFriendListDBResult(db_over, session);
}

void DBResultHandler::HandleDBResult(DBOverlapped* db_over)
{
	switch (db_over->type) {
	case DBOperationType::ADD_FRIEND_REQUEST:
		HandleRequestFriendDBResult(db_over);
		break;
	case DBOperationType::ADD_FRIEND:
		HandleAddFriendDBResult(db_over);
		break;
	case DBOperationType::DELETE_FRIEND:
		HandleDeleteFriendDBResult(db_over);
		break;
	case DBOperationType::LOAD_RANKING:
		server.ProcessRankingResult(db_over);
		break;
	default:
		break;
	}
}

void DBResultHandler::HandleDBResult(DBOverlapped* db_over, Session& session)
{
	switch (db_over->type) {
	case DBOperationType::LOGIN:
		HandleLoginDBResult(db_over, session);
		break;

	case DBOperationType::UPDATE_SCORE:
		HandleUpdateScoreDBResult(db_over, session);
		break;

	case DBOperationType::UPDATE_MATCH_RESULT:
		HandleUpdateMatchResultDBResult(db_over, session);
		break;

	case DBOperationType::ADD_FRIEND_REQUEST:
		HandleRequestFriendDBResult(db_over);
		break;

	case DBOperationType::ADD_FRIEND:
		HandleAddFriendDBResult(db_over);
		break;

	case DBOperationType::DELETE_FRIEND:
		HandleDeleteFriendDBResult(db_over);
		break;

	case DBOperationType::LOAD_FRIEND_LIST:
		HandleLoadFriendListDBResult(db_over, session);
		break;

	case DBOperationType::LOAD_RANKING:
		break;
	}
}
