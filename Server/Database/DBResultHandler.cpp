#include "DBResultHandler.h"
#include "packet_types.h"
#include "IOCPServer.h"
#include <iostream>

DBResultHandler::DBResultHandler(IOCPServer& server) : server_(server)
{
}

void DBResultHandler::HandleRequestFriendDBResult(DBOverlapped* db_over)
{
	if (db_over->result_data->is_success) {
		DBResultAddFriendRequest* res = static_cast<DBResultAddFriendRequest*>(db_over->result_data.get());
		auto recver_session = server_.active_users_.FindSessionById(res->recver_info.id);
		if (recver_session) {
			if (recver_session->GetDBInfo().id != res->recver_info.id) return;

			S2C_REQUEST_FRIEND_PACKET request_p;
			request_p.header.size = static_cast<std::uint16_t>(sizeof(request_p));
			request_p.header.type = S2C_REQUEST_FRIEND;
			request_p.requester_id = res->requester_info.id;
			server_.StringToCharBuf(res->requester_info.nickname, request_p.requester_nickname, MAX_USER_NAME);
			recver_session->SendPacket(reinterpret_cast<char*>(&request_p), server_.GetHandle());
		}
	}
}

void DBResultHandler::HandleAddFriendDBResult(DBOverlapped* db_over)
{
	if (db_over->result_data->is_success) {
		DBResultAddFriend* res = static_cast<DBResultAddFriend*>(db_over->result_data.get());
		server_.SendAddFriendResult(res->requester_info, res->accepter_info);
	}
}

void DBResultHandler::HandleDeleteFriendDBResult(DBOverlapped* db_over)
{
	if (db_over->result_data->is_success) {
		DBResultDeleteFriend* res = static_cast<DBResultDeleteFriend*>(db_over->result_data.get());
		server_.SendDeleteFriendResult(res->requester_id, res->target_id);
	}
}

void DBResultHandler::HandleLoadRankingDBResult(DBOverlapped* db_over)
{
	if (db_over->result_data->is_success) {
		DBResultLoadRanking* res = static_cast<DBResultLoadRanking*>(db_over->result_data.get());
		server_.GetRankingManager().InitRanking(res->rankings);
	}
}

void DBResultHandler::HandleLoginDBResult(DBOverlapped* db_over, const SP<Session>& session)
{
	if (!session) return;

	S2C_LOGIN_PACKET login_p;
	S2C_ERROR_PACKET error_p;
	login_p.header.size = static_cast<std::uint16_t>(sizeof(login_p));
	login_p.header.type = S2C_LOGIN;
	login_p.id = -1;
	if (db_over->result_data->is_success) {
		DBResultLogin* login_result = static_cast<DBResultLogin*>(db_over->result_data.get());

		if (!server_.active_users_.AddUser(session, login_result)) {
			login_p.id = -2;
		}
		else {
			DBResultLogin db_info = session->GetDBInfo();
			login_p.id = db_info.id;
			login_p.max_score = db_info.max_score;
			login_p.win_count = db_info.win_count;
			login_p.lose_count = db_info.lose_count;
			server_.StringToCharBuf(db_info.nickname, login_p.nickname, sizeof(login_p.nickname));
		}
	}

	if (login_p.id == -1) {
		error_p.header.size = static_cast<std::uint16_t>(sizeof(error_p));
		error_p.header.type = S2C_ERROR;
		error_p.error_code = ErrorCode::LOGIN_FAILED;
		session->SendPacket(reinterpret_cast<char*>(&error_p), server_.GetHandle());
	}

	else if (login_p.id == -2) {
		error_p.header.size = static_cast<std::uint16_t>(sizeof(error_p));
		error_p.header.type = S2C_ERROR;
		error_p.error_code = ErrorCode::DUPLICATE_LOGIN_ID;
		session->SendPacket(reinterpret_cast<char*>(&error_p), server_.GetHandle());
	}

	else {
		std::cout << "로그인 - 플레이어: " << session->GetDBInfo().nickname << std::endl;
		session->SendPacket(reinterpret_cast<char*>(&login_p), server_.GetHandle());

		SessionKey key = session->GetSessionKey();
		server_.EnqueueDBTask(std::make_unique<DBLoadFriendListTask>(key), session);
	}
}

void DBResultHandler::HandleUpdateScoreDBResult(DBOverlapped* db_over, const SP<Session>& session)
{
	if (!session) return;
	if (db_over->result_data->is_success) {
		DBResultUpdateScore* res = static_cast<DBResultUpdateScore*>(db_over->result_data.get());
		DBResultLogin db_info = session->UpdateMaxScore(res->max_score);
		server_.GetRankingManager().UpdateRanking(db_info.id, db_info.nickname, res->max_score);
		S2C_UPDATE_SCORE_PACKET us_p;
		us_p.header.size = static_cast<std::uint16_t>(sizeof(us_p));
		us_p.header.type = S2C_UPDATE_SCORE;
		us_p.max_score = res->max_score;
		session->SendPacket(reinterpret_cast<char*>(&us_p), server_.GetHandle());
	}
}

void DBResultHandler::HandleUpdateMatchResultDBResult(DBOverlapped* db_over, const SP<Session>& session)
{
	if (!session) return;
	if (db_over->result_data->is_success) {
		S2C_MATCH_RECORD_PACKET record_p;
		record_p.header.size = static_cast<std::uint16_t>(sizeof(record_p));
		record_p.header.type = S2C_MATCH_RECORD;
		DBResultUpdateMatchResult* res = static_cast<DBResultUpdateMatchResult*>(db_over->result_data.get());
		DBResultLogin db_info = session->UpdateMatchRecord(res->is_winner);

		record_p.win_count = db_info.win_count;
		record_p.lose_count = db_info.lose_count;

		session->SendPacket(reinterpret_cast<char*>(&record_p), server_.GetHandle());
	}
}

void DBResultHandler::HandleLoadFriendListDBResult(DBOverlapped* db_over, const SP<Session>& session)
{
	if (!session) return;
	if (db_over->result_data->is_success) {
		DBResultLoadFriendList* res = static_cast<DBResultLoadFriendList*>(db_over->result_data.get());
		session->InitFriendList(res->friend_list);
	}
}

void DBResultHandler::HandleIOResult(DBOverlapped* db_over, const SP<Session>& session)
{
	if (!session) return;

	switch (db_over->result_data->type) {
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
	}
}

void DBResultHandler::HandleInitServerResult(DBOverlapped* db_over)
{
	switch (db_over->result_data->type){
	case DBOperationType::LOAD_RANKING:
		HandleLoadRankingDBResult(db_over);
		break;
	default:
		break;
	}
}
