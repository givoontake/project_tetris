#pragma once
class IOCPServer;
class Session;
struct DBOverlapped;

class DBResultHandler
{
	IOCPServer& server_;

public:
	DBResultHandler(IOCPServer& server);
	void HandleSessionDBResult(DBOverlapped* db_over, Session* session);
	void HandleServerDBResult(DBOverlapped* db_over);

private:
	void HandleAddFriendRequestDBResult(DBOverlapped* db_over);
	void HandleAddFriendDBResult(DBOverlapped* db_over);
	void HandleDeleteFriendDBResult(DBOverlapped* db_over);
	void HandleLoadRankingsDBResult(DBOverlapped* db_over);
	void HandleLoginDBResult(DBOverlapped* db_over, Session* session);
	void HandleUpdateScoreDBResult(DBOverlapped* db_over, Session* session);
	void HandleUpdateMatchResultDBResult(DBOverlapped* db_over, Session* session);
	void HandleLoadFriendListDBResult(DBOverlapped* db_over, Session* session);
};
