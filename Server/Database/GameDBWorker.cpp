#include <stdexcept>
#include "GameDBWorker.h"
#include "DBResult.h"

namespace
{
    void PostMatchResultCompletions(HANDLE iocp_handle, const DBUpdateMatchResultTask& task, bool is_ok, ULONG_PTR completion_key)
    {
        for (int i = 0; i < task.player_count; ++i)
        {
            if ((task.completion_mask & (1u << i)) == 0) continue;
            auto* db_over = new DBOverlapped{ DBOperationType::UPDATE_MATCH_RESULT };
            db_over->ex_over.op_type = OPType::DB;
            db_over->ex_over.key = task.players[i];
            if (is_ok)
            {
                db_over->result_data = std::make_unique<DBResultUpdateMatchResult>();
                auto* result = static_cast<DBResultUpdateMatchResult*>(db_over->result_data.get());
                result->is_winner = task.players[i].id == task.winner.id;
            }
            PostQueuedCompletionStatus(iocp_handle, static_cast<int>(OPType::DB), completion_key, reinterpret_cast<WSAOVERLAPPED*>(db_over));
        }
    }
}

void GameDBWorker::ExecuteLoadRanking()
{
    auto db_over = std::make_unique<DBOverlapped>(DBOperationType::LOAD_RANKING);
    db_over->ex_over.op_type = OPType::DB;

    try
    {
        auto* stmt = caches_.GetStmt(DBOperationType::LOAD_RANKING);
        if (!stmt)
        {
            const char* SQL_LOAD_RANKING =
                "SELECT user_id, nickname, single_score "
                "FROM users "
                "WHERE single_score > 0 "
                "ORDER BY single_score DESC, user_id ASC "
                "LIMIT 10";

            caches_.stmt_cache[DBOperationType::LOAD_RANKING].reset(caches_.conn->prepareStatement(SQL_LOAD_RANKING));

            stmt = caches_.GetStmt(DBOperationType::LOAD_RANKING);
            if (!stmt)
            {
                PostQueuedCompletionStatus(iocp_handle_, static_cast<int>(OPType::DB), DB_SERVER_COMPLETION, reinterpret_cast<WSAOVERLAPPED*>(db_over.release()));
                return;
            }
        }

        std::unique_ptr<sql::ResultSet> rs(stmt->executeQuery());
        auto result_data = std::make_unique<DBResultLoadRanking>();
        DBResultLoadRanking* res = result_data.get();

        while (rs && rs->next()) {
            RankingInfo info;
            info.id = rs->getInt(1);
            info.nickname = rs->getString(2);
            info.score = rs->getInt(3);
            res->rankings.emplace_back(std::move(info));
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

void GameDBWorker::ExecuteUpdateScore(SessionKey key, int new_score)
{
    auto db_over = std::make_unique<DBOverlapped>(DBOperationType::UPDATE_SCORE);
    db_over->ex_over.op_type = OPType::DB;
    db_over->ex_over.key = key;
    int user_id = key.id;

    try
    {
        auto* stmt = caches_.GetStmt(DBOperationType::UPDATE_SCORE);
        if (!stmt)
        {
            const char* SQL_UPDATE_SCORE =
                "UPDATE users SET single_score=? WHERE user_id=?";

            caches_.stmt_cache[DBOperationType::UPDATE_SCORE].reset(caches_.conn->prepareStatement(SQL_UPDATE_SCORE));
            stmt = caches_.GetStmt(DBOperationType::UPDATE_SCORE);
            if (!stmt)
            {
                PostQueuedCompletionStatus(iocp_handle_, static_cast<int>(OPType::DB), DB_SESSION_COMPLETION, reinterpret_cast<WSAOVERLAPPED*>(db_over.release())); // 전송 바이트는 0만 아니면 됨. 어차피 DB 처리는 전송 바이트 처리 필요 없음
                return;
            }
        }

        // 바인딩
        stmt->setInt(1, new_score);
        stmt->setInt(2, user_id);
        //std::cout << "ExecuteUpdateScore() user_id: " << user_id << std::endl;

        // 실행
        const int affected = stmt->executeUpdate();
        if (affected > 0) {
            // 1이면 업데이트 성공
            //std::cout << "score update success!, new score: " << new_score << std::endl;
            db_over->result_data = std::make_unique<DBResultUpdateScore>();
            DBResultUpdateScore* p = static_cast<DBResultUpdateScore*>(db_over->result_data.get());
            p->max_score = new_score;
        }
        
        else {
            //std::cout << "score update fail!, new score: " << new_score << std::endl;
        }
    }
    catch (const sql::SQLException& e)
    {
        PrintErrorLog(__func__, e);
        throw;
    }

    PostQueuedCompletionStatus(iocp_handle_, static_cast<int>(OPType::DB), DB_SESSION_COMPLETION, reinterpret_cast<WSAOVERLAPPED*>(db_over.release()));
}

void GameDBWorker::ExecuteUpdateMatchResult(const DBUpdateMatchResultTask& task)
{
    bool is_success = false;
    try
    {
        auto* stmt = caches_.GetStmt(DBOperationType::UPDATE_MATCH_RESULT);
        if (!stmt)
        {
            const char* SQL_UPDATE_MATCH_RESULT =
                "UPDATE users "
                "SET win = win + CASE WHEN user_id=? THEN 1 ELSE 0 END, "
                "lose = lose + CASE WHEN user_id=? THEN 0 ELSE 1 END "
                "WHERE user_id IN (?, ?, ?, ?, ?)";
            caches_.stmt_cache[DBOperationType::UPDATE_MATCH_RESULT].reset(caches_.conn->prepareStatement(SQL_UPDATE_MATCH_RESULT));
            stmt = caches_.GetStmt(DBOperationType::UPDATE_MATCH_RESULT);
            if (!stmt)
            {
                PostMatchResultCompletions(iocp_handle_, task, false, DB_SESSION_COMPLETION);
                return;
            }
        }

        stmt->setInt(1, task.winner.id);
        stmt->setInt(2, task.winner.id);
        for (int i = 0; i < MAX_MATCH_RESULT_PLAYERS; ++i) stmt->setInt(i + 3, task.players[i].id);
        is_success = stmt->executeUpdate() == task.player_count;
    }
    catch (const sql::SQLException& e)
    {
        PrintErrorLog(__func__, e);
        throw;
    }

    PostMatchResultCompletions(iocp_handle_, task, is_success, DB_SESSION_COMPLETION);
}

void GameDBWorker::ExecuteAddFriend(SessionKey key, FriendInfo accepter_info, int requester_id)
{
    auto db_over = std::make_unique<DBOverlapped>(DBOperationType::ADD_FRIEND);
    db_over->ex_over.op_type = OPType::DB;
    db_over->ex_over.key = key;
    //db_over->ex_over.request_gen; // 사실 여기서는 의미가 없음. 적용된 두 클라에게 모두 보내야해서 두 클라의 키값이 모두 필요

    try
    {
        caches_.conn->setAutoCommit(false);
        auto* af_stmt = caches_.GetStmt(DBOperationType::ADD_FRIEND);
        if (!af_stmt)
        {
            const char* SQL_ADD_FRIEND = // 쿼리 안에서 몇 개를 요청하던 1번의 요청 결과는 원자적
            "INSERT IGNORE INTO friends (my_id, friend_id) "
            "VALUES (?, ?), (?, ?)";

            caches_.stmt_cache[DBOperationType::ADD_FRIEND].reset(caches_.conn->prepareStatement(SQL_ADD_FRIEND));
            af_stmt = caches_.GetStmt(DBOperationType::ADD_FRIEND);
            if (!af_stmt) goto POST_RESULT;
        }

        af_stmt->setInt(1, accepter_info.id);
        af_stmt->setInt(2, requester_id);
        af_stmt->setInt(3, requester_id);
        af_stmt->setInt(4, accepter_info.id);

        const int af_affected = af_stmt->executeUpdate(); // INSERT, UPDATE, DELETE -> 영향을 받은 행의 수를 반환

		if (af_affected >= 2) // 친구 추가 성공 -> 친구 요청 레코드 삭제 (추가는 양방향이므로 2행이 영향을 받아야 성공)
        {
            const char* SQL_DELETE_FRIEND_REQUEST =
                "DELETE FROM friend_requests "
                "WHERE from_user_id = ? AND to_user_id = ?";

            auto* dfr_stmt = caches_.GetStmt(DBOperationType::DELETE_FRIEND_REQUEST);
            if (!dfr_stmt) {
                caches_.stmt_cache[DBOperationType::DELETE_FRIEND_REQUEST].reset(caches_.conn->prepareStatement(SQL_DELETE_FRIEND_REQUEST));
                dfr_stmt = caches_.GetStmt(DBOperationType::DELETE_FRIEND_REQUEST);
				if (!dfr_stmt) goto POST_RESULT;
            }
            
            dfr_stmt->setInt(1, requester_id);
            dfr_stmt->setInt(2, accepter_info.id);

            int dfr_affected = dfr_stmt->executeUpdate();
            if (dfr_affected > 0) {
                const char* SQL_GET_FRIEND_INFO =
                    "SELECT nickname FROM users WHERE user_id=?";

                auto* gri_stmt = caches_.GetStmt(DBOperationType::GET_FRIEND_INFO);
                if (!gri_stmt) {
                    caches_.stmt_cache[DBOperationType::GET_FRIEND_INFO].reset(caches_.conn->prepareStatement(SQL_GET_FRIEND_INFO));

                    gri_stmt = caches_.GetStmt(DBOperationType::GET_FRIEND_INFO);
                    if (!gri_stmt) goto POST_RESULT;
                }

                gri_stmt->setInt(1, requester_id);
                std::unique_ptr<sql::ResultSet> rs(gri_stmt->executeQuery());

                if (rs && rs->next()) {
                    auto result_data = std::make_unique<DBResultAddFriend>();
                    DBResultAddFriend* p = result_data.get();
                    p->requester_info.nickname = rs->getString(1);
                    p->requester_info.id = requester_id;
                    p->accepter_info = accepter_info;
                    caches_.conn->commit();
                    db_over->result_data = std::move(result_data);
                }
            }
        }

    }
    catch (const sql::SQLException& e)
    {
        PrintErrorLog(__func__, e);
        caches_.conn->rollback();
        caches_.conn->setAutoCommit(true);
        throw;
    }

POST_RESULT:
	if (!db_over->result_data->is_success) caches_.conn->rollback();

    caches_.conn->setAutoCommit(true);
    PostQueuedCompletionStatus(iocp_handle_, static_cast<int>(OPType::DB), DB_SESSION_COMPLETION, reinterpret_cast<WSAOVERLAPPED*>(db_over.release()));
}

void GameDBWorker::ExecuteDeleteFriend(SessionKey key, int target_id)
{
    auto db_over = std::make_unique<DBOverlapped>(DBOperationType::DELETE_FRIEND);
    db_over->ex_over.op_type = OPType::DB;
    db_over->ex_over.key = key;
    //db_over->ex_over.request_gen = key.gen;
    int requester_id = key.id;

    try
    {
        auto* df_stmt = caches_.GetStmt(DBOperationType::DELETE_FRIEND);
        if (!df_stmt)
        {
            const char* SQL_DELETE_FRIEND = // 쿼리 안에서 몇 개를 요청하던 1번의 요청 결과는 원자적
                "DELETE FROM friends "
                "WHERE(my_id, friend_id) IN((? , ?), (? , ?))";

            caches_.stmt_cache[DBOperationType::DELETE_FRIEND].reset(caches_.conn->prepareStatement(SQL_DELETE_FRIEND));

            df_stmt = caches_.GetStmt(DBOperationType::DELETE_FRIEND);
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

        const int affected = df_stmt->executeUpdate(); // INSERT, UPDATE, DELETE -> 영향을 받은 행의 수를 반환

        if (affected >= 2)
        {
            db_over->result_data = std::make_unique<DBResultDeleteFriend>();
            DBResultDeleteFriend* p = static_cast<DBResultDeleteFriend*>(db_over->result_data.get()); 
            p->requester_id = requester_id;
			p->target_id = target_id;
        }
    }
    catch (const sql::SQLException& e)
    {
        PrintErrorLog(__func__, e);
        throw;
    }

    PostQueuedCompletionStatus(iocp_handle_, static_cast<int>(OPType::DB), DB_SESSION_COMPLETION, reinterpret_cast<WSAOVERLAPPED*>(db_over.release()));
}

void GameDBWorker::ExecuteLoadFriendList(SessionKey key)
{
    auto db_over = std::make_unique<DBOverlapped>(DBOperationType::LOAD_FRIEND_LIST);
    db_over->ex_over.op_type = OPType::DB;
    db_over->ex_over.key = key;
    int user_id = key.id;

    try
    {
        auto* stmt = caches_.GetStmt(DBOperationType::LOAD_FRIEND_LIST);
        if (!stmt)
        {
            // SELECT: 컬럼들 선택(열)
            // FROM: 테이블 선택(단일 뿐만 아니라 조인된 테이블도 당연히 가능)
            // WHERE: 테이블에서 조건에 맞는 행 선택
            const char* SQL_GET_FRIEND_LIST = // 쿼리 안에서 몇 개를 요청하던 1번의 요청 결과는 원자적
                "SELECT user_id, nickname " // 헷갈리지만, 직접 해보면 맞다. 친구 목록 뒤에 친구에 대한 부가 정보를 붙이고(친구 닉네임 알려고), 그 중 내 친구들만 골라서 그 중 user_id, nickname을 받는다.
				"FROM friends JOIN users " 
                "ON friends.friend_id = users.user_id "
			    "WHERE my_id = ?";

            caches_.stmt_cache[DBOperationType::LOAD_FRIEND_LIST].reset(caches_.conn->prepareStatement(SQL_GET_FRIEND_LIST));

            stmt = caches_.GetStmt(DBOperationType::LOAD_FRIEND_LIST);
            if (!stmt)
            {
                PostQueuedCompletionStatus(iocp_handle_, static_cast<int>(OPType::DB), DB_SESSION_COMPLETION, reinterpret_cast<WSAOVERLAPPED*>(db_over.release()));
                return;
            }
        }

        stmt->setInt(1, user_id);

        std::unique_ptr<sql::ResultSet> rs(stmt->executeQuery());

        if (rs) {
            auto result_data = std::make_unique<DBResultLoadFriendList>();
            DBResultLoadFriendList* res = result_data.get();
            while (rs->next()) { // rs->next()는 다음 결과로 이동하며, 결과가 있는지 여부를 반환한다.
                FriendInfo info;
				info.id = rs->getInt(1);
                info.nickname = rs->getString(2);
				res->friend_list.emplace_back(info);
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

void GameDBWorker::ExecuteAddFriendRequest(SessionKey key, FriendInfo requester_info, int recver_id)
{
    auto db_over = std::make_unique<DBOverlapped>(DBOperationType::ADD_FRIEND_REQUEST);
    db_over->ex_over.op_type = OPType::DB;
    db_over->ex_over.key = key;

    try
    {
        caches_.conn->setAutoCommit(false);
        auto* afr_stmt = caches_.GetStmt(DBOperationType::ADD_FRIEND_REQUEST);
        if (!afr_stmt)
        {
            const char* SQL_ADD_FRIEND_REQUEST =
                "INSERT INTO friend_requests (from_user_id, to_user_id) "
                "VALUES (?, ?)";
            caches_.stmt_cache[DBOperationType::ADD_FRIEND_REQUEST].reset(caches_.conn->prepareStatement(SQL_ADD_FRIEND_REQUEST));
            afr_stmt = caches_.GetStmt(DBOperationType::ADD_FRIEND_REQUEST);
			if (!afr_stmt) goto POST_RESULT;
        }
        afr_stmt->setInt(1, requester_info.id);
        afr_stmt->setInt(2, recver_id);
        const int afr_affected = afr_stmt->executeUpdate();
        
        if (afr_affected >= 1)
        {
            const char* SQL_GET_FRIEND_INFO =
				"SELECT nickname FROM users WHERE user_id=?";

            auto* gri_stmt = caches_.GetStmt(DBOperationType::GET_FRIEND_INFO);
            if (!gri_stmt) {
                caches_.stmt_cache[DBOperationType::GET_FRIEND_INFO].reset(caches_.conn->prepareStatement(SQL_GET_FRIEND_INFO));

                gri_stmt = caches_.GetStmt(DBOperationType::GET_FRIEND_INFO);
				if (!gri_stmt) goto POST_RESULT;
            }

			gri_stmt->setInt(1, recver_id);
            std::unique_ptr<sql::ResultSet> gri_rs(gri_stmt->executeQuery());

            if (gri_rs && gri_rs->next()) {
                FriendInfo recver_info;
                recver_info.id = recver_id;
                recver_info.nickname = gri_rs->getString(1);

                db_over->result_data = std::make_unique<DBResultAddFriendRequest>();
                DBResultAddFriendRequest* p = static_cast<DBResultAddFriendRequest*>(db_over->result_data.get());
                p->requester_info = requester_info;
                p->recver_info = recver_info; // 얘는 있는지 없는지 모르니까 gen은 당연히 못넣음
            }
        }
    }
    catch (const sql::SQLException& e)
    {
        PrintErrorLog(__func__, e);
        caches_.conn->rollback();
        caches_.conn->setAutoCommit(true);
        throw;
    }
POST_RESULT:
    if (!db_over->result_data->is_success) caches_.conn->rollback();
	caches_.conn->setAutoCommit(true);

	PostQueuedCompletionStatus(iocp_handle_, static_cast<int>(OPType::DB), DB_SESSION_COMPLETION, reinterpret_cast<WSAOVERLAPPED*>(db_over.release()));
}

void GameDBWorker::ProcessTask(DBTask& task)
{
    switch (task.type) {
    case DBOperationType::LOAD_RANKING:
        ExecuteLoadRanking();
        break;
    case DBOperationType::UPDATE_SCORE:
    {
        auto& score_task = static_cast<DBUpdateScoreTask&>(task);
        ExecuteUpdateScore(score_task.key, score_task.new_score);
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
        ExecuteAddFriend(friend_task.key, friend_task.accepter_info, friend_task.requester_id);
        break;
    }
    case DBOperationType::DELETE_FRIEND:
    {
        auto& friend_task = static_cast<DBDeleteFriendTask&>(task);
        ExecuteDeleteFriend(friend_task.key, friend_task.target_id);
        break;
    }
    case DBOperationType::ADD_FRIEND_REQUEST:
    {
        auto& request_task = static_cast<DBAddFriendRequestTask&>(task);
        ExecuteAddFriendRequest(request_task.key, request_task.requester_info, request_task.recver_id);
        break;
    }
    case DBOperationType::LOAD_FRIEND_LIST:
        ExecuteLoadFriendList(static_cast<DBLoadFriendListTask&>(task).key);
        break;
    default:
        throw std::runtime_error("unsupported game database task type");
    }
}
