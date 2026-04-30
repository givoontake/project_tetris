// Database.cpp
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cctype>
#include <botan/bcrypt.h>
#include <jdbc/cppconn/connection.h>
#include <jdbc/cppconn/driver.h>
#include <jdbc/cppconn/metadata.h>
#include "Database.h"
#include "DBResult.h"

#undef min

// -------------------- 유틸(로컬) --------------------
static inline void Trim(std::string& s)
{
    auto not_space = [](unsigned char ch) { return !std::isspace(ch); };

    s.erase(s.begin(), std::find_if(s.begin(), s.end(), not_space));
    s.erase(std::find_if(s.rbegin(), s.rend(), not_space).base(), s.end());
}

static inline void PrintDBConnectionInfo(sql::Connection* conn)
{
    if (!conn)
    {
        std::cout << "[DB] Connection is null\n";
        return;
    }

    std::cout << "===== DB Connection Info =====\n";
    std::cout << "Connected        : " << (conn->isValid() ? "YES" : "NO") << '\n';
    std::cout << "Server Version   : " << conn->getMetaData()->getDatabaseProductVersion() << '\n';
    std::cout << "Server Name      : " << conn->getMetaData()->getDatabaseProductName() << '\n';
    std::cout << "User Name        : " << conn->getMetaData()->getUserName() << '\n';
    std::cout << "Current Schema   : " << conn->getSchema() << '\n';
    std::cout << "Auto Commit      : " << (conn->getAutoCommit() ? "ON" : "OFF") << '\n';
    std::cout << "===============================\n";
}

namespace
{
    void PrintErrorLog(const char* func_name, const std::exception& e)
    {
        std::cerr << "[ERROR] in " << func_name << "\n"
            << "  type: " << typeid(e).name() << "\n"
            << "  what(): " << e.what() << std::endl;
    }

    void PrintErrorLog(const char* func_name, const sql::SQLException& e)
    {
        std::cerr << "[DB][SQLException] in " << func_name << "\n"
            << "  what(): " << e.what() << "\n"
            << "  errorCode: " << e.getErrorCode() << "\n"
            << "  SQLState: " << e.getSQLState() << std::endl;
    }

    void PrintErrorLog(const char* func_name)
    {
        std::cerr << "[ERROR] in " << func_name
            << "  (unknown exception)" << std::endl;
    }
}

// -------------------- Database --------------------

Database::Database()
{
    LoadDBConfigFromFile("db_config.txt");
}

Database::~Database()
{
}

void Database::init(HANDLE iocp)
{
    iocp_handle = iocp;
}

bool Database::Enqueue(Task db_task, const SP<Session>& session)
{
    if (session && !session->TryAddPending()) return false;
    try {
    {
        std::lock_guard<std::mutex> lock(mutex);
        pending_db_tasks.push(std::move(db_task));
    }
    }
    catch (...) {
        if (session) session->ReducePending();
        return false;
    }
    cv.notify_one();
    return true;
}

bool Database::LoadDBConfigFromFile(const std::string& file_path)
{
    std::ifstream file(file_path);
    if (!file.is_open())
        return false;

    std::string line;
    while (std::getline(file, line))
    {
        // CR 제거(윈도우 \r\n 대응)
        if (!line.empty() && line.back() == '\r')
            line.pop_back();

        // 주석은 남겨둔다.
        Trim(line);
        if (line.empty() || line[0] == '#')
            continue;

        auto pos = line.find('=');
        if (pos == std::string::npos)
            continue;

        std::string key = line.substr(0, pos);
        std::string val = line.substr(pos + 1);
        Trim(key);
        Trim(val);

        if (key == "host")
            db_host = val;
        else if (key == "port")
        {
            try
            {
                int p = std::stoi(val); // 문자열을 그대로 정수 변환, 예외를 던질 수 있음
                // 포트 범위 예외 처리
                if (p < 0) p = 0;
                if (p > 65535) p = 65535;
                db_port = static_cast<uint16_t>(p);
            }
            catch (...)
            {
                db_port = 0;
                PrintErrorLog(__func__);
            }
        }
        else if (key == "user")
            db_id = val;
        else if (key == "password")
            db_password = val;
        //else if (key == "schema") schema = val;
    }

    return true;
}

// ---- DB 스레드에서 실행될 "실제 DB 작업" ----
// 외부는 보통 아래처럼 람다로 감싸 Enqueue:
//   db.Enqueue([&db, sid, id, pw]{ db.ExecuteLogin(sid, id, pw); });
void Database::ExecuteLogin(SessionKey key, const std::string login_id, const std::string password)
{
    auto* db_over = new DBOverlapped{};
    db_over->ex_over.op_type = OP_TYPE::DB;
    db_over->ex_over.key = key;
    db_over->type = DBOperationType::LOGIN;
    db_over->ok = false; // 기본 실패로 두고, 성공 조건에서만 true

    try
    {
        // 1) PreparedStatement 확보 (캐시 없으면 준비)
        auto* stmt = caches.GetStmt(DBOperationType::LOGIN);
        if (!stmt) // 캐시가 없으면 캐시를 만들고 다시 캐시를 가져오고, 그래도 없으면 실패 처리
        {
            // EXISTS: 항상 1행 1컬럼(0/1) 반환
            const char* SQL_LOGIN =
                "SELECT password_hash "
                "FROM users "
                "WHERE login_id=? "
                "LIMIT 1";

            caches.stmt_cache[DBOperationType::LOGIN].reset(caches.conn->prepareStatement(SQL_LOGIN));
            stmt = caches.GetStmt(DBOperationType::LOGIN);
            if (!stmt)
            {
                db_over->ok = false;
                PostQueuedCompletionStatus(iocp_handle, static_cast<int>(OP_TYPE::DB), DB_IO_COMPLETION, reinterpret_cast<WSAOVERLAPPED*>(db_over));
                return;
            }
        }

        // 2) 바인딩
        stmt->setString(1, login_id);

        // 3) 실행
        std::unique_ptr<sql::ResultSet> rs(stmt->executeQuery()); // 요청한 쿼리에 대한 결과 집합 객체

        // 4) 결과
        if (rs && rs->next()) // rs는 결과 집합 객체가 존재하는지, rs->next()는 첫 번째 결과가 존재하는지, 논리상 해시된 비밀번호를 가져오므로 rs->next()는 반드시 존재
        {
            // 그래서 첫 결과가 존재한다면, 값을 읽어 실제 존재 여부를 판정
            const std::string password_hash = rs->getString(1);
            if (Botan::check_bcrypt(password, password_hash))
            {
                db_over->ok = true;
            }
            else
            {
                /*std::cerr << "잘못된 비밀번호입니다. " << std::endl;
                std::cerr << "id: " << login_id << std::endl;
                std::cerr << "pw: " << password << std::endl;*/
                db_over->ok = false;
            }
        }
        else
        {
            //std::cerr << "일치하는 아이디가 없습니다. id: " << login_id << std::endl;
            db_over->ok = false;
        }
    }
    catch (const sql::SQLException& e)
    {
        PrintErrorLog(__func__, e);
        db_over->ok = false;
    }

    if (db_over->ok)
    {
        try
        {
            db_over->result_data = std::make_unique<DBResultLogin>(); // 동적할당 및 객체 수명관리 시작
            DBResultLogin* p = static_cast<DBResultLogin*>(db_over->result_data.get()); // 값 조작용 raw 포인터

            auto* info_stmt = caches.GetStmt(DBOperationType::LOAD_SESSION_INFO); // LOAD_SESSION_INFO는 캐시에만 활용
            if (!info_stmt)
            {
                const char* SQL_LOAD_INFO =
                    "SELECT user_id, nickname, single_score, win, lose "
                    "FROM users "
                    "WHERE login_id=? "
                    "LIMIT 1";

                caches.stmt_cache[DBOperationType::LOAD_SESSION_INFO]
                    .reset(caches.conn->prepareStatement(SQL_LOAD_INFO));

                info_stmt = caches.GetStmt(DBOperationType::LOAD_SESSION_INFO);
                if (!info_stmt)
                {
                    db_over->ok = false;
                }
            }

            if (db_over->ok)
            {
                info_stmt->setString(1, login_id);
                std::unique_ptr<sql::ResultSet> info_rs(info_stmt->executeQuery());

                if (info_rs && info_rs->next()) // 가져온 결과(행)이 있는지 판별
                {
                    p->login_id = login_id;
					p->id = info_rs->getInt(1);
                    p->nickname = info_rs->getString(2);
                    p->max_score = info_rs->getInt(3);
                    p->win_count = info_rs->getInt(4);
                    p->lose_count = info_rs->getInt(5);        

                    //std::cout << "로그인 성공. " << std::endl;
                    //std::cout << "id: " << login_id << std::endl;
                    ////std::cout << "pw: " << password << std::endl;
                    //std::cout << "nickname: " << p->nickname << std::endl;
                }
                else
                {
                    db_over->ok = false;
                }
            }
        }
        catch (const sql::SQLException& e)
        {
            PrintErrorLog(__func__, e);
            db_over->result_data = nullptr;
            db_over->ok = false;
        }
    }

    PostQueuedCompletionStatus(iocp_handle, static_cast<int>(OP_TYPE::DB), DB_IO_COMPLETION, reinterpret_cast<WSAOVERLAPPED*>(db_over));
}

void Database::ExecuteLoadRanking()
{
    auto* db_over = new DBOverlapped{};
    db_over->ex_over.op_type = OP_TYPE::DB;
    db_over->type = DBOperationType::LOAD_RANKING;
    db_over->ok = false;

    try
    {
        auto* stmt = caches.GetStmt(DBOperationType::LOAD_RANKING);
        if (!stmt)
        {
            const char* SQL_LOAD_RANKING =
                "SELECT user_id, nickname, single_score "
                "FROM users "
                "WHERE single_score > 0 "
                "ORDER BY single_score DESC, user_id ASC "
                "LIMIT 10";

            caches.stmt_cache[DBOperationType::LOAD_RANKING]
                .reset(caches.conn->prepareStatement(SQL_LOAD_RANKING));

            stmt = caches.GetStmt(DBOperationType::LOAD_RANKING);
            if (!stmt)
            {
                PostQueuedCompletionStatus(iocp_handle, static_cast<int>(OP_TYPE::DB), DB_IO_COMPLETION, reinterpret_cast<WSAOVERLAPPED*>(db_over));
                return;
            }
        }

        std::unique_ptr<sql::ResultSet> rs(stmt->executeQuery());
        db_over->result_data = std::make_unique<DBResultLoadRanking>();
        DBResultLoadRanking* res = static_cast<DBResultLoadRanking*>(db_over->result_data.get());

        while (rs && rs->next()) {
            RankingInfo info;
            info.id = rs->getInt(1);
            info.nickname = rs->getString(2);
            info.score = rs->getInt(3);
            res->rankings.emplace_back(std::move(info));
        }

        db_over->ok = true;
    }
    catch (const sql::SQLException& e)
    {
        PrintErrorLog(__func__, e);
        db_over->ok = false;
    }

    PostQueuedCompletionStatus(iocp_handle, static_cast<int>(OP_TYPE::DB), DB_INIT_SERVER_COMPLETION, reinterpret_cast<WSAOVERLAPPED*>(db_over));
}

void Database::ExecuteUpdateScore(SessionKey key, int new_score)
{
    auto* db_over = new DBOverlapped{};
    db_over->ex_over.op_type = OP_TYPE::DB;
    db_over->ex_over.key = key;
    db_over->type = DBOperationType::UPDATE_SCORE;
    db_over->ok = false;
    int user_id = key.id;

    try
    {
        auto* stmt = caches.GetStmt(DBOperationType::UPDATE_SCORE);
        if (!stmt)
        {
            const char* SQL_UPDATE_SCORE =
                "UPDATE users SET single_score=? WHERE user_id=?";

            caches.stmt_cache[DBOperationType::UPDATE_SCORE].reset(caches.conn->prepareStatement(SQL_UPDATE_SCORE));
            stmt = caches.GetStmt(DBOperationType::UPDATE_SCORE);
            if (!stmt)
            {
                PostQueuedCompletionStatus(iocp_handle, static_cast<int>(OP_TYPE::DB), DB_IO_COMPLETION, reinterpret_cast<WSAOVERLAPPED*>(db_over)); // 전송 바이트는 0만 아니면 됨. 어차피 DB 처리는 전송 바이트 처리 필요 없음
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
            db_over->ok = true;
            db_over->result_data = std::make_unique<DBResultUpdateScore>();
            DBResultUpdateScore* p = static_cast<DBResultUpdateScore*>(db_over->result_data.get());
            p->max_score = new_score;
        }
        
        else {
            db_over->ok = false;
            //std::cout << "score update fail!, new score: " << new_score << std::endl;
        }
    }
    catch (const sql::SQLException& e)
    {
        PrintErrorLog(__func__, e);
        db_over->ok = false;
    }

    PostQueuedCompletionStatus(iocp_handle, static_cast<int>(OP_TYPE::DB), DB_IO_COMPLETION, reinterpret_cast<WSAOVERLAPPED*>(db_over));
}

void Database::ExecuteUpdateMatchResult(SessionKey key, bool is_winner)
{
    auto* db_over = new DBOverlapped{};
    db_over->ex_over.op_type = OP_TYPE::DB;
    db_over->ex_over.key = key;
    db_over->type = DBOperationType::UPDATE_MATCH_RESULT;
    db_over->ok = false;
    int user_id = key.id;

    try
    {
        auto* stmt = caches.GetStmt(DBOperationType::UPDATE_MATCH_RESULT);
        if (!stmt)
        {
            const char* SQL_UPDATE_MATCH_RESULT =
                "UPDATE users "
                "SET win = win + ?, "
                "lose = lose + ? "
                "WHERE user_id=?";

            caches.stmt_cache[DBOperationType::UPDATE_MATCH_RESULT]
                .reset(caches.conn->prepareStatement(SQL_UPDATE_MATCH_RESULT));

            stmt = caches.GetStmt(DBOperationType::UPDATE_MATCH_RESULT);
            if (!stmt)
            {
                PostQueuedCompletionStatus(iocp_handle, static_cast<int>(OP_TYPE::DB), DB_IO_COMPLETION, reinterpret_cast<WSAOVERLAPPED*>(db_over));
                return;
            }
        }

        const int win_delta = is_winner ? 1 : 0;
        const int lose_delta = is_winner ? 0 : 1;

        stmt->setInt(1, win_delta);
        stmt->setInt(2, lose_delta);
        stmt->setInt(3, user_id);

        const int affected = stmt->executeUpdate();

        if (affected > 0) {
            //std::cout << "match_result update success! " << std::endl;
            db_over->ok = true;
            db_over->result_data = std::make_unique<DBResultUpdateMatchResult>();
            DBResultUpdateMatchResult* p = static_cast<DBResultUpdateMatchResult*>(db_over->result_data.get());
			p->is_winner = is_winner;
        }

        else db_over->ok = false;           
    }

    catch (const sql::SQLException& e)
    {
        PrintErrorLog(__func__, e);
        db_over->ok = false;
    }

    PostQueuedCompletionStatus(iocp_handle, static_cast<int>(OP_TYPE::DB), DB_IO_COMPLETION, reinterpret_cast<WSAOVERLAPPED*>(db_over));
}

void Database::ExecuteAddFriend(SessionKey key, FriendInfo accepter_info, int requester_id)
{
    auto* db_over = new DBOverlapped{};
    db_over->ex_over.op_type = OP_TYPE::DB;
    db_over->ex_over.key = key;
    //db_over->ex_over.request_gen; // 사실 여기서는 의미가 없음. 적용된 두 클라에게 모두 보내야해서 두 클라의 키값이 모두 필요
    db_over->type = DBOperationType::ADD_FRIEND;
    db_over->ok = false;

    caches.conn->setAutoCommit(false);
    try
    {
        auto* af_stmt = caches.GetStmt(DBOperationType::ADD_FRIEND);
        if (!af_stmt)
        {
            const char* SQL_ADD_FRIEND = // 쿼리 안에서 몇 개를 요청하던 1번의 요청 결과는 원자적
            "INSERT IGNORE INTO friends (my_id, friend_id) "
            "VALUES (?, ?), (?, ?)";

            caches.stmt_cache[DBOperationType::ADD_FRIEND].reset(caches.conn->prepareStatement(SQL_ADD_FRIEND));
            af_stmt = caches.GetStmt(DBOperationType::ADD_FRIEND);
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

            auto* dfr_stmt = caches.GetStmt(DBOperationType::DELETE_FRIEND_REQUEST);
            if (!dfr_stmt) {
                caches.stmt_cache[DBOperationType::DELETE_FRIEND_REQUEST].reset(caches.conn->prepareStatement(SQL_DELETE_FRIEND_REQUEST));
                dfr_stmt = caches.GetStmt(DBOperationType::DELETE_FRIEND_REQUEST);
				if (!dfr_stmt) goto POST_RESULT;
            }
            
            dfr_stmt->setInt(1, requester_id);
            dfr_stmt->setInt(2, accepter_info.id);

            int dfr_affected = dfr_stmt->executeUpdate();
            if (dfr_affected > 0) {
                const char* SQL_GET_FRIEND_INFO =
                    "SELECT nickname FROM users WHERE user_id=?";

                auto* gri_stmt = caches.GetStmt(DBOperationType::GET_FRIEND_INFO);
                if (!gri_stmt) {
                    caches.stmt_cache[DBOperationType::GET_FRIEND_INFO].reset(caches.conn->prepareStatement(SQL_GET_FRIEND_INFO));

                    gri_stmt = caches.GetStmt(DBOperationType::GET_FRIEND_INFO);
                    if (!gri_stmt) goto POST_RESULT;
                }

                gri_stmt->setInt(1, requester_id);
                std::unique_ptr<sql::ResultSet> rs(gri_stmt->executeQuery());

                if (rs && rs->next()) {
                    db_over->ok = true;
                    db_over->result_data = std::make_unique<DBResultAddFriend>();
                    DBResultAddFriend* p = static_cast<DBResultAddFriend*>(db_over->result_data.get());
                    p->requester_info.nickname = rs->getString(1);
                    p->requester_info.id = requester_id;
                    p->accepter_info = accepter_info;
                    caches.conn->commit();
                }
            }
        }

    }
    catch (const sql::SQLException& e)
    {
        PrintErrorLog(__func__, e);
        db_over->ok = false;
    }

POST_RESULT:
	if (!db_over->ok) caches.conn->rollback();

    caches.conn->setAutoCommit(true);
    PostQueuedCompletionStatus(iocp_handle, static_cast<int>(OP_TYPE::DB), DB_IO_COMPLETION, reinterpret_cast<WSAOVERLAPPED*>(db_over));
}

void Database::ExecuteDeleteFriend(SessionKey key, int target_id)
{
    auto* db_over = new DBOverlapped{};
    db_over->ex_over.op_type = OP_TYPE::DB;
    db_over->ex_over.key = key;
    //db_over->ex_over.request_gen = key.gen;
    db_over->type = DBOperationType::DELETE_FRIEND;
    db_over->ok = false;
    int requester_id = key.id;

    try
    {
        auto* df_stmt = caches.GetStmt(DBOperationType::DELETE_FRIEND);
        if (!df_stmt)
        {
            const char* SQL_DELETE_FRIEND = // 쿼리 안에서 몇 개를 요청하던 1번의 요청 결과는 원자적
                "DELETE FROM friends "
                "WHERE(my_id, friend_id) IN((? , ?), (? , ?))";

            caches.stmt_cache[DBOperationType::DELETE_FRIEND].reset(caches.conn->prepareStatement(SQL_DELETE_FRIEND));

            df_stmt = caches.GetStmt(DBOperationType::DELETE_FRIEND);
            if (!df_stmt)
            {
                PostQueuedCompletionStatus(iocp_handle, static_cast<int>(OP_TYPE::DB), DB_IO_COMPLETION, reinterpret_cast<WSAOVERLAPPED*>(db_over));
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
            db_over->ok = true;
            db_over->result_data = std::make_unique<DBResultDeleteFriend>();
            DBResultDeleteFriend* p = static_cast<DBResultDeleteFriend*>(db_over->result_data.get()); 
            p->requester_id = requester_id;
			p->target_id = target_id;
        }
    }
    catch (const sql::SQLException& e)
    {
        PrintErrorLog(__func__, e);
        db_over->ok = false;
    }

    PostQueuedCompletionStatus(iocp_handle, static_cast<int>(OP_TYPE::DB), DB_IO_COMPLETION, reinterpret_cast<WSAOVERLAPPED*>(db_over));
}

void Database::ExecuteLoadFriendList(SessionKey key)
{
    auto* db_over = new DBOverlapped{};
    db_over->ex_over.op_type = OP_TYPE::DB;
    db_over->ex_over.key = key;
    db_over->type = DBOperationType::LOAD_FRIEND_LIST;
    db_over->ok = false;
    int user_id = key.id;

    try
    {
        auto* stmt = caches.GetStmt(DBOperationType::LOAD_FRIEND_LIST);
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

            caches.stmt_cache[DBOperationType::LOAD_FRIEND_LIST].reset(caches.conn->prepareStatement(SQL_GET_FRIEND_LIST));

            stmt = caches.GetStmt(DBOperationType::LOAD_FRIEND_LIST);
            if (!stmt)
            {
                PostQueuedCompletionStatus(iocp_handle, static_cast<int>(OP_TYPE::DB), DB_IO_COMPLETION, reinterpret_cast<WSAOVERLAPPED*>(db_over));
                return;
            }
        }

        stmt->setInt(1, user_id);

        std::unique_ptr<sql::ResultSet> rs(stmt->executeQuery());

        if (rs) {
            db_over->ok = true;
            db_over->result_data = std::make_unique<DBResultLoadFriendList>();
            DBResultLoadFriendList* res = static_cast<DBResultLoadFriendList*>(db_over->result_data.get());
            while (rs->next()) { // rs->next()는 다음 결과로 이동하며, 결과가 있는지 여부를 반환한다.
                FriendInfo info;
				info.id = rs->getInt(1);
                info.nickname = rs->getString(2);
				res->friend_list.emplace_back(info);
            }
        }
    }
    catch (const sql::SQLException& e)
    {
        PrintErrorLog(__func__, e);
        db_over->ok = false;
    }

    PostQueuedCompletionStatus(iocp_handle, static_cast<int>(OP_TYPE::DB), DB_IO_COMPLETION, reinterpret_cast<WSAOVERLAPPED*>(db_over));
}

void Database::ExecuteAddFriendRequest(SessionKey key, FriendInfo requester_info, int recver_id)
{
    auto* db_over = new DBOverlapped{};
    db_over->ex_over.op_type = OP_TYPE::DB;
    db_over->ex_over.key = key;
    db_over->type = DBOperationType::ADD_FRIEND_REQUEST;
    db_over->ok = false;

	caches.conn->setAutoCommit(false);
    try
    {
        auto* afr_stmt = caches.GetStmt(DBOperationType::ADD_FRIEND_REQUEST);
        if (!afr_stmt)
        {
            const char* SQL_ADD_FRIEND_REQUEST =
                "INSERT INTO friend_requests (from_user_id, to_user_id) "
                "VALUES (?, ?)";
            caches.stmt_cache[DBOperationType::ADD_FRIEND_REQUEST].reset(caches.conn->prepareStatement(SQL_ADD_FRIEND_REQUEST));
            afr_stmt = caches.GetStmt(DBOperationType::ADD_FRIEND_REQUEST);
			if (!afr_stmt) goto POST_RESULT;
        }
        afr_stmt->setInt(1, requester_info.id);
        afr_stmt->setInt(2, recver_id);
        const int afr_affected = afr_stmt->executeUpdate();
        
        if (afr_affected >= 1)
        {
            const char* SQL_GET_FRIEND_INFO =
				"SELECT nickname FROM users WHERE user_id=?";

            auto* gri_stmt = caches.GetStmt(DBOperationType::GET_FRIEND_INFO);
            if (!gri_stmt) {
                caches.stmt_cache[DBOperationType::GET_FRIEND_INFO].reset(caches.conn->prepareStatement(SQL_GET_FRIEND_INFO));

                gri_stmt = caches.GetStmt(DBOperationType::GET_FRIEND_INFO);
				if (!gri_stmt) goto POST_RESULT;
            }

			gri_stmt->setInt(1, recver_id);
            std::unique_ptr<sql::ResultSet> gri_rs(gri_stmt->executeQuery());

            if (gri_rs && gri_rs->next()) {
                FriendInfo recver_info;
                recver_info.id = recver_id;
                recver_info.nickname = gri_rs->getString(1);

                db_over->ok = true;
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
        db_over->ok = false;
    }
POST_RESULT:
    if (!db_over->ok) caches.conn->rollback();
	caches.conn->setAutoCommit(true);

	PostQueuedCompletionStatus(iocp_handle, static_cast<int>(OP_TYPE::DB), DB_IO_COMPLETION, reinterpret_cast<WSAOVERLAPPED*>(db_over));
}

// ---- DB 스레드 루프 ----
void Database::Run()
{
    // 1) 커넥션 오픈 + stmt 캐시 eager 준비
    // 실패 시에도 루프는 돌되, 작업 수행은 stmt nullptr로 실패 처리될 것.
    if (!Connect())
    {
        std::cerr << "[DB] Failed to connect to database." << std::endl;
        return;
    }

    running = true;

    while (true)
    {
        Task job;
        {
            std::unique_lock<std::mutex> lock(mutex);
            // 잠자고 있는 상태에서는 깨우는 신호가 오면 다시 조건을 검사한다.
            cv.wait(lock, [this]() {
                return !pending_db_tasks.empty() || !running.load(std::memory_order_acquire); // 조건이 참이 되어야 깨어나므로 running = false이면 깨어나도록 해야함
                });

            if (!running.load(std::memory_order_acquire) &&
                pending_db_tasks.empty())
                break;

            job = std::move(pending_db_tasks.front());
            pending_db_tasks.pop();
        }

        // 2) 작업 실행 (DB 실행은 락 없이)
        job();
    }

    // 3) 정리
    Disconnect();
}

bool Database::Connect()
{
    try
    {
        sql::mysql::MySQL_Driver* driver =
            sql::mysql::get_mysql_driver_instance();

        // Connector/C++ legacy는 보통 tcp://host:port
        const std::string url =
            "tcp://" + db_host + ":" + std::to_string(db_port);

        caches.conn.reset(driver->connect(url, db_id, db_password));
        caches.conn->setSchema("tetris");

        PrintDBConnectionInfo(caches.conn.get());

        return true;
    }
    catch (const sql::SQLException& e)
    {
        PrintErrorLog(__func__, e);
        return false;
    }
}

void Database::Disconnect()
{
    caches.stmt_cache.clear();

    if (caches.conn)
    {
        try { caches.conn->close(); }
        catch (...) {}
        caches.conn.reset();
    }
}
