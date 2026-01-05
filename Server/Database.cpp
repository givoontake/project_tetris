// Database.cpp
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cctype>
#include <botan/bcrypt.h>
#include "Database.h"

// -------------------- 유틸(로컬) --------------------
static inline void Trim(std::string& s)
{
    auto not_space = [](unsigned char ch) { return !std::isspace(ch); };

    s.erase(s.begin(), std::find_if(s.begin(), s.end(), not_space));
    s.erase(std::find_if(s.rbegin(), s.rend(), not_space).base(), s.end());
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

Database::Database(HANDLE iocp_handle)
    : iocp(iocp_handle)
{
    LoadDBConfigFromFile("db_config.txt");
}

Database::~Database()
{
}

void Database::Enqueue(Task job)
{
    {
        std::lock_guard<std::mutex> lock(mutex);
        pending_db_tasks.push(std::move(job));
    }
    cv.notify_one();
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
void Database::ExecuteLogin(int session_id, const std::string& login_id, const std::string& password)
{
    auto* db_over = new DBOverlapped{};
    db_over->type = DBOperationType::LOGIN;
    db_over->opration_id = session_id;
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
                PostQueuedCompletionStatus(iocp, 0, session_id, reinterpret_cast<WSAOVERLAPPED*>(db_over));
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
                // 비밀번호 불일치
                db_over->ok = false;
            }
        }
        else
        {
            // 아이디가 존재하지 않음
            db_over->ok = false;
        }
    }
    catch (const sql::SQLException& e)
    {
        PrintErrorLog(__func__, e);
        db_over->ok = false;
    }

    PostQueuedCompletionStatus(iocp, 0, session_id, reinterpret_cast<WSAOVERLAPPED*>(db_over));
}

void Database::ExecuteUpdateScore(int session_id, const std::string& login_id, int32_t new_score)
{
    auto* db_over = new DBOverlapped{};
    db_over->type = DBOperationType::SCORE_UPDATE;
    db_over->opration_id = session_id;
    db_over->ok = false;

    try
    {
        auto* stmt = caches.GetStmt(DBOperationType::SCORE_UPDATE);
        if (!stmt)
        {
            const char* SQL_UPDATE_SCORE =
                "UPDATE users SET single_score=? WHERE login_id=?";

            caches.stmt_cache[DBOperationType::SCORE_UPDATE].reset(caches.conn->prepareStatement(SQL_UPDATE_SCORE));
            stmt = caches.GetStmt(DBOperationType::SCORE_UPDATE);
            if (!stmt)
            {
                PostQueuedCompletionStatus(iocp, 0, session_id, reinterpret_cast<WSAOVERLAPPED*>(db_over));
                return;
            }
        }

        // 바인딩
        stmt->setInt(1, new_score);
        stmt->setString(2, login_id);

        // 실행
        const int affected = stmt->executeUpdate();
        if (affected > 0) // 1이면 업데이트 성공
            db_over->ok = true;
        else
            db_over->ok = false;
    }
    catch (const sql::SQLException& e)
    {
        PrintErrorLog(__func__, e);
        db_over->ok = false;
    }

    PostQueuedCompletionStatus(iocp, 0, session_id, reinterpret_cast<WSAOVERLAPPED*>(db_over));
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

    while (true)
    {
        Task job;

        {
            std::unique_lock<std::mutex> lock(mutex);
            // 잠자고 있는 상태에서는 깨우는 신호가 오면 다시 조건을 검사한다.
            cv.wait(lock, [this]() {
                return !pending_db_tasks.empty() ||
                    !running.load(std::memory_order_acquire);
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
