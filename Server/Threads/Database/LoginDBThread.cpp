#include <botan/bcrypt.h>
#include "LoginDBThread.h"
#include "DBResult.h"

void LoginDBThread::ExecuteLogin(SessionKey session_key, const std::string& login_id, const std::string& password)
{
    auto db_over = std::make_unique<DBOverlapped>(DBOperationType::LOGIN); // 기본 실패로 두고, 성공 조건에서만 true
    db_over->ex_over.op_type = OPType::DB;
    db_over->ex_over.session_key = session_key;

    try
    {
        // 1) PreparedStatement 확보 (캐시 없으면 준비)
		auto* stmt = connection_context_.GetStatement(DBOperationType::LOGIN);
        if (!stmt) // 캐시가 없으면 캐시를 만들고 다시 캐시를 가져오고, 그래도 없으면 실패 처리
        {
            const char* SQL_LOGIN =
                "SELECT player_id, nickname, password_hash, single_score, win, lose "
                "FROM players "
                "WHERE login_id=? "
                "LIMIT 1";

			connection_context_.statement_cache[DBOperationType::LOGIN].reset(connection_context_.connection->prepareStatement(SQL_LOGIN));
			stmt = connection_context_.GetStatement(DBOperationType::LOGIN);
            if (!stmt)
            {
                PostQueuedCompletionStatus(iocp_handle_, static_cast<int>(OPType::DB), DB_SESSION_COMPLETION, reinterpret_cast<WSAOVERLAPPED*>(db_over.release()));
                return;
            }
        }

        // 2) 바인딩
        stmt->setString(1, login_id);

        // 3) 실행
        std::unique_ptr<sql::ResultSet> rs(stmt->executeQuery()); // 요청한 쿼리에 대한 결과 집합 객체

        // 4) 결과
        if (rs && rs->next())
        {
            const std::string password_hash = rs->getString(3);
            if (Botan::check_bcrypt(password, password_hash))
            {
                auto result_data = std::make_unique<DBResultLogin>(); // 동적할당 및 객체 수명관리 시작
                DBResultLogin* result = result_data.get(); // 값 조작용 raw 포인터
				result->player_id = rs->getInt(1);
                result->login_id = login_id;
                result->nickname = rs->getString(2);
                result->max_score = rs->getInt(4);
                result->win_count = rs->getInt(5);
                result->lose_count = rs->getInt(6);
                db_over->result_data = std::move(result_data);
            }
        }
    }
    catch (const sql::SQLException& e)
    {
        PrintErrorLog(__func__, e);
        throw;
    }

    PostQueuedCompletionStatus(iocp_handle_, static_cast<int>(OPType::DB), DB_SESSION_COMPLETION, reinterpret_cast<WSAOVERLAPPED*>(db_over.release()));
}

void LoginDBThread::ProcessTask(DBTask& task)
{
    auto& login_task = static_cast<DBLoginTask&>(task);
    ExecuteLogin(login_task.session_key, login_task.login_id, login_task.password);
}
