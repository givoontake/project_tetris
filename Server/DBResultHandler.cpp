#include "DBResultHandler.h"
#include "packet_types.h"
#include "IOCPServer.h"

DBResultHandler::DBResultHandler(IOCPServer& server) : server(server)
{
}

void DBResultHandler::HandleRequestFriendDBResult(DBOverlapped* db_over)
{
	if (db_over->ok) {
		DBResultAddFriendRequest* res = static_cast<DBResultAddFriendRequest*>(db_over->result_data.get());
		int recver_index = server.FindSessionIndexById(res->recver_info.id);
		if (recver_index != -1) {
			Session& recver_session = server.FindSessionByIndex(recver_index);
			std::lock_guard<std::mutex> lock(recver_session.GetMutex());
			if (recver_session.GetState() != SESS_STATE::LOBBY) return;
			if (recver_session.GetDBInfo().id != res->recver_info.id) return;

			S2C_REQUEST_FRIEND_PACKET request_p;
			request_p.header.size = static_cast<std::uint16_t>(sizeof(request_p));
			request_p.header.type = S2C_REQUEST_FRIEND;
			request_p.requester_id = res->requester_info.id;
			server.StringToCharBuf(res->requester_info.nickname, request_p.requester_nickname, MAX_USER_NAME);
			recver_session.SendPacket(reinterpret_cast<char*>(&request_p), server.GetHandle());
		}
	}
}

void DBResultHandler::HandleAddFriendDBResult(DBOverlapped* db_over)
{
	if (db_over->ok) {
		DBResultAddFriend* res = static_cast<DBResultAddFriend*>(db_over->result_data.get());
		server.SendAddFriendResult(res->requester_info, res->accepter_info);
	}
}

void DBResultHandler::HandleDeleteFriendDBResult(DBOverlapped* db_over)
{
	if (db_over->ok) {
		DBResultDeleteFriend* res = static_cast<DBResultDeleteFriend*>(db_over->result_data.get());
		server.SendDeleteFriendResult(res->requester_id, res->target_id);
	}
}

void DBResultHandler::HandleLoadRankingDBResult(DBOverlapped* db_over)
{
	if (db_over->ok && db_over->result_data) {
		DBResultLoadRanking* res = static_cast<DBResultLoadRanking*>(db_over->result_data.get());
		server.GetRankingManager().InitRanking(res->rankings);
	}
}

void DBResultHandler::HandleLoginDBResult(DBOverlapped* db_over, Session& session)
{
	int request_gen = db_over->ex_over.key.gen;
	S2C_LOGIN_PACKET login_p;
	S2C_ERROR_PACKET error_p;
	login_p.header.size = static_cast<std::uint16_t>(sizeof(login_p));
	login_p.header.type = S2C_LOGIN;
	if (db_over->ok) {
		if (db_over->result_data) {
			if (server.CheckDuplicateLoginId(static_cast<DBResultLogin*>(db_over->result_data.get())->login_id)) login_p.id = -2;
			else {
				std::lock_guard<std::mutex> lock(session.GetMutex());
				if (request_gen != session.GetSessionKey().gen) return;
				if (session.GetState() == SESS_STATE::NONE) return;

				session.InitDBInfo(static_cast<DBResultLogin*>(db_over->result_data.get()));
				session.StoreState(SESS_STATE::LOBBY);
				server.active_users.AddUser(session.GetDBInfo().id, session.GetSessionKey().index);
				login_p.id = session.GetDBInfo().id;
				login_p.max_score = session.GetDBInfo().max_score;
				login_p.win_count = session.GetDBInfo().win_count;
				login_p.lose_count = session.GetDBInfo().lose_count;
				server.StringToCharBuf(session.GetDBInfo().nickname, login_p.nickname, sizeof(login_p.nickname));
			}
		}
		else {
			login_p.id = -1;
		}
	}
	else {
		login_p.id = -1;
	}

	if (login_p.id == -1) {
		error_p.header.size = static_cast<std::uint16_t>(sizeof(error_p));
		error_p.header.type = S2C_ERROR;
		error_p.error_code = ERROR_CODE::LOGIN_FAILED;
		session.SendPacket(request_gen, reinterpret_cast<char*>(&error_p), server.GetHandle());
	}

	else if (login_p.id == -2) {
		error_p.header.size = static_cast<std::uint16_t>(sizeof(error_p));
		error_p.header.type = S2C_ERROR;
		error_p.error_code = ERROR_CODE::DUPLICATE_LOGIN_ID;
		session.SendPacket(request_gen, reinterpret_cast<char*>(&error_p), server.GetHandle());
	}

	else {
		session.SendPacket(request_gen, reinterpret_cast<char*>(&login_p), server.GetHandle());

		Database& repr_db = server.GetDB();
		std::lock_guard<std::mutex> lock(session.GetMutex());
		if (session.GetState() == SESS_STATE::NONE) return;
		if (session.GetSessionKey().gen != request_gen) return;
		SessionKey key = session.GetSessionKey();
		int user_id = session.GetDBInfo().id;
		auto task = [&repr_db, key, user_id]() {
			repr_db.ExecuteLoadFriendList(key, user_id);
			};
		repr_db.Enqueue(task);
	}
}

void DBResultHandler::HandleUpdateScoreDBResult(DBOverlapped* db_over, Session& session)
{
	int request_gen = db_over->ex_over.key.gen;
	if (db_over->ok) {
		DBResultUpdateScore* res = static_cast<DBResultUpdateScore*>(db_over->result_data.get());
		{
			std::lock_guard<std::mutex> lock(session.GetMutex());
			if (request_gen != session.GetSessionKey().gen) return;
			if (session.GetState() == SESS_STATE::NONE) return;
			session.GetDBInfo().max_score = res->max_score;
			server.GetRankingManager().UpdateRanking(session.GetDBInfo().id, session.GetDBInfo().nickname, res->max_score);
		}
		S2C_UPDATE_SCORE_PACKET us_p;
		us_p.header.size = static_cast<std::uint16_t>(sizeof(us_p));
		us_p.header.type = S2C_UPDATE_SCORE;
		us_p.max_score = res->max_score;
		session.SendPacket(request_gen, reinterpret_cast<char*>(&us_p), server.GetHandle());
	}
}

void DBResultHandler::HandleUpdateMatchResultDBResult(DBOverlapped* db_over, Session& session)
{
	int request_gen = db_over->ex_over.key.gen;
	if (db_over->ok) {
		S2C_MATCH_RECORD_PACKET record_p;
		record_p.header.size = static_cast<std::uint16_t>(sizeof(record_p));
		record_p.header.type = S2C_MATCH_RECORD;
		DBResultUpdateMatchResult* res = static_cast<DBResultUpdateMatchResult*>(db_over->result_data.get());
		{
			std::lock_guard<std::mutex> lock(session.GetMutex());
			if (request_gen != session.GetSessionKey().gen) return;
			if (session.GetState() == SESS_STATE::NONE) return;

			if (res->is_winner) ++session.GetDBInfo().win_count;
			else ++session.GetDBInfo().lose_count;

			record_p.win_count = session.GetDBInfo().win_count;
			record_p.lose_count = session.GetDBInfo().lose_count;
		}

		session.SendPacket(request_gen, reinterpret_cast<char*>(&record_p), server.GetHandle());
	}
}

void DBResultHandler::HandleLoadFriendListDBResult(DBOverlapped* db_over, Session& session)
{
	int request_gen = db_over->ex_over.key.gen;
	if (db_over->ok) {
		DBResultLoadFriendList* res = static_cast<DBResultLoadFriendList*>(db_over->result_data.get());
		std::lock_guard<std::mutex> lock(session.GetMutex());
		if (session.GetState() == SESS_STATE::NONE) return;
		if (request_gen != session.GetSessionKey().gen) return;
		session.InitFriendList(res->friend_list);
	}
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
		HandleLoadRankingDBResult(db_over);
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
