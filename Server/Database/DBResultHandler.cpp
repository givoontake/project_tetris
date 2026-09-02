#include "DBResultHandler.h"
#include "packet_types.h"
#include "IOCPServer.h"
#include <iostream>

DBResultHandler::DBResultHandler(IOCPServer& server) : server_(server)
{
}

void DBResultHandler::HandleAddFriendRequestDBResult(DBOverlapped* db_over)
{
	if (db_over->result_data->is_success) {
		DBResultAddFriendRequest* result = static_cast<DBResultAddFriendRequest*>(db_over->result_data.get());
		auto receiver_session = server_.active_players_.FindSessionByID(result->receiver_info.player_id);
		if (receiver_session) {
			if (receiver_session->GetDBInfo().player_id != result->receiver_info.player_id) return;

			S2C_ADD_FRIEND_REQUEST_PACKET request_p;
			request_p.header.size = static_cast<std::uint16_t>(sizeof(request_p));
			request_p.header.type = S2C_ADD_FRIEND_REQUEST;
			request_p.requester_id = result->requester_info.player_id;
			server_.StringToCharBuf(result->requester_info.nickname, request_p.requester_nickname, MAX_PLAYER_NAME_SIZE);
			receiver_session->SendPacket(reinterpret_cast<char*>(&request_p), server_.GetIOCPHandle());
		}
	}
}

void DBResultHandler::HandleAddFriendDBResult(DBOverlapped* db_over)
{
	if (db_over->result_data->is_success) {
		DBResultAddFriend* result = static_cast<DBResultAddFriend*>(db_over->result_data.get());
		server_.SendAddFriendResult(result->requester_info, result->acceptor_info);
	}
}

void DBResultHandler::HandleDeleteFriendDBResult(DBOverlapped* db_over)
{
	if (db_over->result_data->is_success) {
		DBResultDeleteFriend* result = static_cast<DBResultDeleteFriend*>(db_over->result_data.get());
		server_.SendDeleteFriendResult(result->requester_id, result->target_id);
	}
}

void DBResultHandler::HandleLoadRankingsDBResult(DBOverlapped* db_over)
{
	if (db_over->result_data->is_success) {
		DBResultLoadRankings* result = static_cast<DBResultLoadRankings*>(db_over->result_data.get());
		server_.GetRankingManager().InitRanking(result->rankings);
	}
}

void DBResultHandler::HandleLoginDBResult(DBOverlapped* db_over, const SP<Session>& session)
{
	if (!session) return;

	S2C_LOGIN_PACKET login_p;
	S2C_ERROR_PACKET error_p;
	login_p.header.size = static_cast<std::uint16_t>(sizeof(login_p));
	login_p.header.type = S2C_LOGIN;
	login_p.player_id = -1;
	if (db_over->result_data->is_success) {
		DBResultLogin* login_result = static_cast<DBResultLogin*>(db_over->result_data.get());

		if (!server_.active_players_.AddPlayer(session, login_result)) {
			login_p.player_id = -2;
		}
		else {
			DBResultLogin db_info = session->GetDBInfo();
			login_p.player_id = db_info.player_id;
			login_p.max_score = db_info.max_score;
			login_p.win_count = db_info.win_count;
			login_p.lose_count = db_info.lose_count;
			server_.StringToCharBuf(db_info.nickname, login_p.nickname, sizeof(login_p.nickname));
		}
	}

	if (login_p.player_id == -1) {
		error_p.header.size = static_cast<std::uint16_t>(sizeof(error_p));
		error_p.header.type = S2C_ERROR;
		error_p.error_code = ErrorCode::LOGIN_FAILED;
		session->SendPacket(reinterpret_cast<char*>(&error_p), server_.GetIOCPHandle());
	}

	else if (login_p.player_id == -2) {
		error_p.header.size = static_cast<std::uint16_t>(sizeof(error_p));
		error_p.header.type = S2C_ERROR;
		error_p.error_code = ErrorCode::DUPLICATE_LOGIN_ID;
		session->SendPacket(reinterpret_cast<char*>(&error_p), server_.GetIOCPHandle());
	}

	else {
		std::cout << "로그인 - 플레이어: " << session->GetDBInfo().nickname << std::endl;
		session->SendPacket(reinterpret_cast<char*>(&login_p), server_.GetIOCPHandle());

		SessionKey session_key = session->GetSessionKey();
		server_.EnqueueDBTask(std::make_unique<DBLoadFriendListTask>(session_key), session);
	}
}

void DBResultHandler::HandleUpdateScoreDBResult(DBOverlapped* db_over, const SP<Session>& session)
{
	if (!session) return;
	if (db_over->result_data->is_success) {
		DBResultUpdateScore* result = static_cast<DBResultUpdateScore*>(db_over->result_data.get());
		DBResultLogin db_info = session->UpdateMaxScore(result->max_score);
		server_.GetRankingManager().UpdateRanking(db_info.player_id, db_info.nickname, result->max_score);
		S2C_UPDATE_SCORE_PACKET us_p;
		us_p.header.size = static_cast<std::uint16_t>(sizeof(us_p));
		us_p.header.type = S2C_UPDATE_SCORE;
		us_p.max_score = result->max_score;
		session->SendPacket(reinterpret_cast<char*>(&us_p), server_.GetIOCPHandle());
	}
}

void DBResultHandler::HandleUpdateMatchResultDBResult(DBOverlapped* db_over, const SP<Session>& session)
{
	if (!session) return;
	if (db_over->result_data->is_success) {
		S2C_MATCH_RECORD_PACKET record_p;
		record_p.header.size = static_cast<std::uint16_t>(sizeof(record_p));
		record_p.header.type = S2C_MATCH_RECORD;
		DBResultUpdateMatchResult* result = static_cast<DBResultUpdateMatchResult*>(db_over->result_data.get());
		DBResultLogin db_info = session->UpdateMatchRecord(result->is_winner);

		record_p.win_count = db_info.win_count;
		record_p.lose_count = db_info.lose_count;

		session->SendPacket(reinterpret_cast<char*>(&record_p), server_.GetIOCPHandle());
	}
}

void DBResultHandler::HandleLoadFriendListDBResult(DBOverlapped* db_over, const SP<Session>& session)
{
	if (!session) return;
	if (db_over->result_data->is_success) {
		DBResultLoadFriendList* result = static_cast<DBResultLoadFriendList*>(db_over->result_data.get());
		session->InitFriendList(result->friend_list);
	}
}

void DBResultHandler::HandleSessionDBResult(DBOverlapped* db_over, const SP<Session>& session)
{
	if (!session) return;

	switch (db_over->result_data->operation_type) {
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
		HandleAddFriendRequestDBResult(db_over);
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

void DBResultHandler::HandleServerDBResult(DBOverlapped* db_over)
{
	switch (db_over->result_data->operation_type){
	case DBOperationType::LOAD_RANKINGS:
		HandleLoadRankingsDBResult(db_over);
		break;
	default:
		break;
	}
}
