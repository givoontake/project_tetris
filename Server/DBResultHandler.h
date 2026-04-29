#pragma once
#include "Types.h"

class IOCPServer;
class Session;
struct DBOverlapped;

class DBResultHandler
{
	IOCPServer& server;

public:
	DBResultHandler(IOCPServer& server);
	void HandleIOResult(DBOverlapped* db_over, const SP<Session>& session);
	void HandleInitServerResult(DBOverlapped* db_over);

private:
	void HandleRequestFriendDBResult(DBOverlapped* db_over);
	void HandleAddFriendDBResult(DBOverlapped* db_over);
	void HandleDeleteFriendDBResult(DBOverlapped* db_over);
	void HandleLoadRankingDBResult(DBOverlapped* db_over);
	void HandleLoginDBResult(DBOverlapped* db_over, const SP<Session>& session);
	void HandleUpdateScoreDBResult(DBOverlapped* db_over, Session& session);
	void HandleUpdateMatchResultDBResult(DBOverlapped* db_over, Session& session);
	void HandleLoadFriendListDBResult(DBOverlapped* db_over, Session& session);
};
