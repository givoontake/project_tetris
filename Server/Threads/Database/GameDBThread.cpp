#include <stdexcept>
#include "GameDBThread.h"
#include "DBResult.h"

namespace
{
	void PostMatchResultCompletions(HANDLE iocp_handle, const DBUpdateMatchResultTask& task, bool is_success, ULONG_PTR completion_key)
    {
        for (int i = 0; i < task.player_count; ++i)
        {
            auto* db_over = new DBOverlapped{ DBOperationType::UPDATE_MATCH_RESULT };
            db_over->ex_over.op_type = OPType::DB;
            db_over->ex_over.session_key = task.player_keys[i];
			if (is_success)
            {
                db_over->result_data = std::make_unique<DBResultUpdateMatchResult>();
                auto* result = static_cast<DBResultUpdateMatchResult*>(db_over->result_data.get());
				result->is_winner = task.player_keys[i].player_id == task.winner_key.player_id;
            }
            PostQueuedCompletionStatus(iocp_handle, static_cast<int>(OPType::DB), completion_key, reinterpret_cast<WSAOVERLAPPED*>(db_over));
        }
    }
}

void GameDBThread::ExecuteLoadRankings()
{
	auto db_over = std::make_unique<DBOverlapped>(DBOperationType::LOAD_RANKINGS);
    db_over->ex_over.op_type = OPType::DB;

    try
    {
		auto* stmt = connection_context_.GetStatement(DBOperationType::LOAD_RANKINGS);
        if (!stmt)
        {
            const char* SQL_LOAD_RANKINGS =
                "SELECT player_id, nickname, single_score "
                "FROM players "
                "WHERE single_score > 0 "
                "ORDER BY single_score DESC, player_id ASC "
                "LIMIT 10";

			connection_context_.statement_cache[DBOperationType::LOAD_RANKINGS].reset(connection_context_.connection->prepareStatement(SQL_LOAD_RANKINGS));

			stmt = connection_context_.GetStatement(DBOperationType::LOAD_RANKINGS);
            if (!stmt)
            {
                PostQueuedCompletionStatus(iocp_handle_, static_cast<int>(OPType::DB), DB_SERVER_COMPLETION, reinterpret_cast<WSAOVERLAPPED*>(db_over.release()));
                return;
            }
        }

        std::unique_ptr<sql::ResultSet> rs(stmt->executeQuery());
		auto result_data = std::make_unique<DBResultLoadRankings>();
		DBResultLoadRankings* result = result_data.get();

        while (rs && rs->next()) {
            RankingInfo info;
			info.player_id = rs->getInt(1);
            info.nickname = rs->getString(2);
            info.score = rs->getInt(3);
            result->rankings.emplace_back(std::move(info));
        }

        db_over->result_data = std::move(result_data);
    }
    catch (const sql::SQLException& e)
    {
        PrintErrorLog(__func__, e);
        throw;
    }

    PostQueuedCompletionStatus(iocp_handle_, static_cast<int>(OPType::DB), DB_SERVER_COMPLETION, reinterpret_cast<WSAOVERLAPPED*>(db_over.release()));
}

void GameDBThread::ExecuteUpdateScore(SessionKey session_key, int new_score)
{
    auto db_over = std::make_unique<DBOverlapped>(DBOperationType::UPDATE_SCORE);
    db_over->ex_over.op_type = OPType::DB;
    db_over->ex_over.session_key = session_key;
	int player_id = session_key.player_id;

    try
    {
        auto* stmt = connection_context_.GetStatement(DBOperationType::UPDATE_SCORE);
        if (!stmt)
        {
            const char* SQL_UPDATE_SCORE =
                "UPDATE players SET single_score=? WHERE player_id=?";

            connection_context_.statement_cache[DBOperationType::UPDATE_SCORE].reset(connection_context_.connection->prepareStatement(SQL_UPDATE_SCORE));
            stmt = connection_context_.GetStatement(DBOperationType::UPDATE_SCORE);
            if (!stmt)
            {
                // DB 완료 경로는 전송 바이트 값을 사용하지 않는다.
                PostQueuedCompletionStatus(iocp_handle_, static_cast<int>(OPType::DB), DB_SESSION_COMPLETION, reinterpret_cast<WSAOVERLAPPED*>(db_over.release()));
                return;
            }
        }

        stmt->setInt(1, new_score);
        stmt->setInt(2, player_id);
        const int affected = stmt->executeUpdate();
        if (affected > 0) {
            db_over->result_data = std::make_unique<DBResultUpdateScore>();
			DBResultUpdateScore* result = static_cast<DBResultUpdateScore*>(db_over->result_data.get());
			result->max_score = new_score;
        }
    }
    catch (const sql::SQLException& e)
    {
        PrintErrorLog(__func__, e);
        throw;
    }

    PostQueuedCompletionStatus(iocp_handle_, static_cast<int>(OPType::DB), DB_SESSION_COMPLETION, reinterpret_cast<WSAOVERLAPPED*>(db_over.release()));
}

void GameDBThread::ExecuteUpdateMatchResult(const DBUpdateMatchResultTask& task)
{
    bool is_success = false;
    try
    {
        auto* stmt = connection_context_.GetStatement(DBOperationType::UPDATE_MATCH_RESULT);
        if (!stmt)
        {
            const char* SQL_UPDATE_MATCH_RESULT =
                "UPDATE players "
                "SET win = win + CASE WHEN player_id=? THEN 1 ELSE 0 END, "
                "lose = lose + CASE WHEN player_id=? THEN 0 ELSE 1 END "
                "WHERE player_id IN (?, ?, ?, ?, ?)";
            connection_context_.statement_cache[DBOperationType::UPDATE_MATCH_RESULT].reset(connection_context_.connection->prepareStatement(SQL_UPDATE_MATCH_RESULT));
            stmt = connection_context_.GetStatement(DBOperationType::UPDATE_MATCH_RESULT);
            if (!stmt)
            {
                PostMatchResultCompletions(iocp_handle_, task, false, DB_SESSION_COMPLETION);
                return;
            }
        }

		stmt->setInt(1, task.winner_key.player_id);
		stmt->setInt(2, task.winner_key.player_id);
		for (int i = 0; i < MAX_MATCH_RESULT_PLAYERS; ++i) stmt->setInt(i + 3, task.player_keys[i].player_id);
        is_success = stmt->executeUpdate() == task.player_count;
    }
    catch (const sql::SQLException& e)
    {
        PrintErrorLog(__func__, e);
        throw;
    }

    PostMatchResultCompletions(iocp_handle_, task, is_success, DB_SESSION_COMPLETION);
}

void GameDBThread::ExecuteAddFriend(SessionKey session_key, FriendInfo acceptor_info, int requester_id)
{
    auto db_over = std::make_unique<DBOverlapped>(DBOperationType::ADD_FRIEND);
    db_over->ex_over.op_type = OPType::DB;
    db_over->ex_over.session_key = session_key;

    try
    {
        connection_context_.connection->setAutoCommit(false);
        auto* af_stmt = connection_context_.GetStatement(DBOperationType::ADD_FRIEND);
        if (!af_stmt)
        {
            const char* SQL_ADD_FRIEND = // 쿼리 안에서 몇 개를 요청하던 1번의 요청 결과는 원자적
            "INSERT IGNORE INTO friends (my_id, friend_id) "
            "VALUES (?, ?), (?, ?)";

            connection_context_.statement_cache[DBOperationType::ADD_FRIEND].reset(connection_context_.connection->prepareStatement(SQL_ADD_FRIEND));
            af_stmt = connection_context_.GetStatement(DBOperationType::ADD_FRIEND);
            if (!af_stmt) goto POST_RESULT;
        }

		af_stmt->setInt(1, acceptor_info.player_id);
        af_stmt->setInt(2, requester_id);
        af_stmt->setInt(3, requester_id);
		af_stmt->setInt(4, acceptor_info.player_id);

        const int af_affected = af_stmt->executeUpdate();

		if (af_affected >= 2) // 친구 추가 성공 -> 친구 요청 레코드 삭제 (추가는 양방향이므로 2행이 영향을 받아야 성공)
        {
            const char* SQL_DELETE_FRIEND_REQUEST =
                "DELETE FROM friend_requests "
                "WHERE from_player_id = ? AND to_player_id = ?";

            auto* dfr_stmt = connection_context_.GetStatement(DBOperationType::DELETE_FRIEND_REQUEST);
            if (!dfr_stmt) {
                connection_context_.statement_cache[DBOperationType::DELETE_FRIEND_REQUEST].reset(connection_context_.connection->prepareStatement(SQL_DELETE_FRIEND_REQUEST));
                dfr_stmt = connection_context_.GetStatement(DBOperationType::DELETE_FRIEND_REQUEST);
				if (!dfr_stmt) goto POST_RESULT;
            }
            
            dfr_stmt->setInt(1, requester_id);
			dfr_stmt->setInt(2, acceptor_info.player_id);

            int dfr_affected = dfr_stmt->executeUpdate();
            if (dfr_affected > 0) {
                const char* SQL_GET_FRIEND_INFO =
                    "SELECT nickname FROM players WHERE player_id=?";

                auto* gri_stmt = connection_context_.GetStatement(DBOperationType::GET_FRIEND_INFO);
                if (!gri_stmt) {
                    connection_context_.statement_cache[DBOperationType::GET_FRIEND_INFO].reset(connection_context_.connection->prepareStatement(SQL_GET_FRIEND_INFO));

                    gri_stmt = connection_context_.GetStatement(DBOperationType::GET_FRIEND_INFO);
                    if (!gri_stmt) goto POST_RESULT;
                }

                gri_stmt->setInt(1, requester_id);
                std::unique_ptr<sql::ResultSet> rs(gri_stmt->executeQuery());

                if (rs && rs->next()) {
                    auto result_data = std::make_unique<DBResultAddFriend>();
					DBResultAddFriend* result = result_data.get();
					result->requester_info.nickname = rs->getString(1);
					result->requester_info.player_id = requester_id;
					result->acceptor_info = acceptor_info;
                    connection_context_.connection->commit();
                    db_over->result_data = std::move(result_data);
                }
            }
        }

    }
    catch (const sql::SQLException& e)
    {
        PrintErrorLog(__func__, e);
        connection_context_.connection->rollback();
        connection_context_.connection->setAutoCommit(true);
        throw;
    }

POST_RESULT:
	if (!db_over->result_data->is_success) connection_context_.connection->rollback();

    connection_context_.connection->setAutoCommit(true);
    PostQueuedCompletionStatus(iocp_handle_, static_cast<int>(OPType::DB), DB_SESSION_COMPLETION, reinterpret_cast<WSAOVERLAPPED*>(db_over.release()));
}

void GameDBThread::ExecuteDeleteFriend(SessionKey session_key, int target_id)
{
    auto db_over = std::make_unique<DBOverlapped>(DBOperationType::DELETE_FRIEND);
    db_over->ex_over.op_type = OPType::DB;
    db_over->ex_over.session_key = session_key;
	int requester_id = session_key.player_id;

    try
    {
        auto* df_stmt = connection_context_.GetStatement(DBOperationType::DELETE_FRIEND);
        if (!df_stmt)
        {
            const char* SQL_DELETE_FRIEND = // 쿼리 안에서 몇 개를 요청하던 1번의 요청 결과는 원자적
                "DELETE FROM friends "
                "WHERE(my_id, friend_id) IN((? , ?), (? , ?))";

            connection_context_.statement_cache[DBOperationType::DELETE_FRIEND].reset(connection_context_.connection->prepareStatement(SQL_DELETE_FRIEND));

            df_stmt = connection_context_.GetStatement(DBOperationType::DELETE_FRIEND);
            if (!df_stmt)
            {
                PostQueuedCompletionStatus(iocp_handle_, static_cast<int>(OPType::DB), DB_SESSION_COMPLETION, reinterpret_cast<WSAOVERLAPPED*>(db_over.release()));
                return;
            }
        }

        df_stmt->setInt(1, requester_id);
        df_stmt->setInt(2, target_id);
        df_stmt->setInt(3, target_id);
        df_stmt->setInt(4, requester_id);

        const int affected = df_stmt->executeUpdate();

        if (affected >= 2)
        {
            db_over->result_data = std::make_unique<DBResultDeleteFriend>();
			DBResultDeleteFriend* result = static_cast<DBResultDeleteFriend*>(db_over->result_data.get()); 
			result->requester_id = requester_id;
			result->target_id = target_id;
        }
    }
    catch (const sql::SQLException& e)
    {
        PrintErrorLog(__func__, e);
        throw;
    }

    PostQueuedCompletionStatus(iocp_handle_, static_cast<int>(OPType::DB), DB_SESSION_COMPLETION, reinterpret_cast<WSAOVERLAPPED*>(db_over.release()));
}

void GameDBThread::ExecuteLoadFriendList(SessionKey session_key)
{
    auto db_over = std::make_unique<DBOverlapped>(DBOperationType::LOAD_FRIEND_LIST);
    db_over->ex_over.op_type = OPType::DB;
    db_over->ex_over.session_key = session_key;
	int player_id = session_key.player_id;

    try
    {
        auto* stmt = connection_context_.GetStatement(DBOperationType::LOAD_FRIEND_LIST);
        if (!stmt)
        {
            const char* SQL_GET_FRIEND_LIST = // 쿼리 안에서 몇 개를 요청하던 1번의 요청 결과는 원자적
				"SELECT player_id, nickname "
				"FROM friends JOIN players " 
                "ON friends.friend_id = players.player_id "
			    "WHERE my_id = ?";

            connection_context_.statement_cache[DBOperationType::LOAD_FRIEND_LIST].reset(connection_context_.connection->prepareStatement(SQL_GET_FRIEND_LIST));

            stmt = connection_context_.GetStatement(DBOperationType::LOAD_FRIEND_LIST);
            if (!stmt)
            {
                PostQueuedCompletionStatus(iocp_handle_, static_cast<int>(OPType::DB), DB_SESSION_COMPLETION, reinterpret_cast<WSAOVERLAPPED*>(db_over.release()));
                return;
            }
        }

        stmt->setInt(1, player_id);

        std::unique_ptr<sql::ResultSet> rs(stmt->executeQuery());

        if (rs) {
            auto result_data = std::make_unique<DBResultLoadFriendList>();
            DBResultLoadFriendList* result = result_data.get();
            while (rs->next()) {
                FriendInfo info;
				info.player_id = rs->getInt(1);
                info.nickname = rs->getString(2);
				result->friend_list.emplace_back(info);
            }
            db_over->result_data = std::move(result_data);
        }
    }
    catch (const sql::SQLException& e)
    {
        PrintErrorLog(__func__, e);
        throw;
    }

    PostQueuedCompletionStatus(iocp_handle_, static_cast<int>(OPType::DB), DB_SESSION_COMPLETION, reinterpret_cast<WSAOVERLAPPED*>(db_over.release()));
}

void GameDBThread::ExecuteAddFriendRequest(SessionKey session_key, FriendInfo requester_info, int receiver_id)
{
    auto db_over = std::make_unique<DBOverlapped>(DBOperationType::ADD_FRIEND_REQUEST);
    db_over->ex_over.op_type = OPType::DB;
    db_over->ex_over.session_key = session_key;

    try
    {
        connection_context_.connection->setAutoCommit(false);
        auto* afr_stmt = connection_context_.GetStatement(DBOperationType::ADD_FRIEND_REQUEST);
        if (!afr_stmt)
        {
            const char* SQL_ADD_FRIEND_REQUEST =
                "INSERT INTO friend_requests (from_player_id, to_player_id) "
                "VALUES (?, ?)";
            connection_context_.statement_cache[DBOperationType::ADD_FRIEND_REQUEST].reset(connection_context_.connection->prepareStatement(SQL_ADD_FRIEND_REQUEST));
            afr_stmt = connection_context_.GetStatement(DBOperationType::ADD_FRIEND_REQUEST);
			if (!afr_stmt) goto POST_RESULT;
        }
		afr_stmt->setInt(1, requester_info.player_id);
        afr_stmt->setInt(2, receiver_id);
        const int afr_affected = afr_stmt->executeUpdate();
        
        if (afr_affected >= 1)
        {
            const char* SQL_GET_FRIEND_INFO =
				"SELECT nickname FROM players WHERE player_id=?";

            auto* gri_stmt = connection_context_.GetStatement(DBOperationType::GET_FRIEND_INFO);
            if (!gri_stmt) {
                connection_context_.statement_cache[DBOperationType::GET_FRIEND_INFO].reset(connection_context_.connection->prepareStatement(SQL_GET_FRIEND_INFO));

                gri_stmt = connection_context_.GetStatement(DBOperationType::GET_FRIEND_INFO);
				if (!gri_stmt) goto POST_RESULT;
            }

			gri_stmt->setInt(1, receiver_id);
            std::unique_ptr<sql::ResultSet> gri_rs(gri_stmt->executeQuery());

            if (gri_rs && gri_rs->next()) {
                FriendInfo receiver_info;
			receiver_info.player_id = receiver_id;
                receiver_info.nickname = gri_rs->getString(1);

                db_over->result_data = std::make_unique<DBResultAddFriendRequest>();
				DBResultAddFriendRequest* result = static_cast<DBResultAddFriendRequest*>(db_over->result_data.get());
				result->requester_info = requester_info;
				result->receiver_info = receiver_info; // 얘는 있는지 없는지 모르니까 gen은 당연히 못넣음
            }
        }
    }
    catch (const sql::SQLException& e)
    {
        PrintErrorLog(__func__, e);
        connection_context_.connection->rollback();
        connection_context_.connection->setAutoCommit(true);
        throw;
    }
POST_RESULT:
    if (!db_over->result_data->is_success) connection_context_.connection->rollback();
	connection_context_.connection->setAutoCommit(true);

	PostQueuedCompletionStatus(iocp_handle_, static_cast<int>(OPType::DB), DB_SESSION_COMPLETION, reinterpret_cast<WSAOVERLAPPED*>(db_over.release()));
}

void GameDBThread::ProcessTask(DBTask& task)
{
    switch (task.operation_type) {
	case DBOperationType::LOAD_RANKINGS:
		ExecuteLoadRankings();
        break;
    case DBOperationType::UPDATE_SCORE:
    {
        auto& score_task = static_cast<DBUpdateScoreTask&>(task);
        ExecuteUpdateScore(score_task.session_key, score_task.new_score);
        break;
    }
    case DBOperationType::UPDATE_MATCH_RESULT:
    {
        auto& match_task = static_cast<DBUpdateMatchResultTask&>(task);
        ExecuteUpdateMatchResult(match_task);
        break;
    }
    case DBOperationType::ADD_FRIEND:
    {
        auto& friend_task = static_cast<DBAddFriendTask&>(task);
        ExecuteAddFriend(friend_task.session_key, friend_task.acceptor_info, friend_task.requester_id);
        break;
    }
    case DBOperationType::DELETE_FRIEND:
    {
        auto& friend_task = static_cast<DBDeleteFriendTask&>(task);
        ExecuteDeleteFriend(friend_task.session_key, friend_task.target_id);
        break;
    }
    case DBOperationType::ADD_FRIEND_REQUEST:
    {
        auto& request_task = static_cast<DBAddFriendRequestTask&>(task);
        ExecuteAddFriendRequest(request_task.session_key, request_task.requester_info, request_task.receiver_id);
        break;
    }
    case DBOperationType::LOAD_FRIEND_LIST:
        ExecuteLoadFriendList(static_cast<DBLoadFriendListTask&>(task).session_key);
        break;
    default:
        throw std::runtime_error("unsupported game database task type");
    }
}
