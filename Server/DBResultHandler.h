#pragma once

class IOCPServer;
class Session;
struct DBOverlapped;

class DBResultHandler
{
	IOCPServer& server;

public:
	DBResultHandler(IOCPServer& server);
	void HandleDBResult(DBOverlapped* db_over);
	void HandleDBResult(DBOverlapped* db_over, Session& session);

private:
	void HandleRequestFriendDBResult(DBOverlapped* db_over);
	void HandleAddFriendDBResult(DBOverlapped* db_over);
	void HandleDeleteFriendDBResult(DBOverlapped* db_over);
	void HandleLoginDBResult(DBOverlapped* db_over, Session& session);
	void HandleUpdateScoreDBResult(DBOverlapped* db_over, Session& session);
	void HandleUpdateMatchResultDBResult(DBOverlapped* db_over, Session& session);
	void HandleLoadFriendListDBResult(DBOverlapped* db_over, Session& session);
};
