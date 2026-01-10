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
void Database::ExecuteLogin(int session_id, int session_index, const std::string& login_id, const std::string& password)
{
    auto* db_over = new DBOverlapped{};
    db_over->ex_over.op_type = DB;
    db_over->type = DBOperationType::LOGIN;
    db_over->ex_over.operation_id = session_id;
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
                PostQueuedCompletionStatus(iocp_handle, DB, session_index, reinterpret_cast<WSAOVERLAPPED*>(db_over));
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
                std::cerr << "잘못된 비밀번호입니다. " << std::endl;
                std::cerr << "id: " << login_id << std::endl;
                std::cerr << "pw: " << password << std::endl;
                db_over->ok = false;
            }
        }
        else
        {
            std::cerr << "일치하는 아이디가 없습니다. id: " << login_id << std::endl;
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
            // 1) LoginInfo 동적 할당
            auto* info = new DBInfo{};

            // 2) PreparedStatement 확보 (세션 정보 로드)
            auto* info_stmt = caches.GetStmt(DBOperationType::LOAD_SESSION_INFO); // LOAD_SESSION_INFO는 캐시에만 활용
            if (!info_stmt)
            {
                const char* SQL_LOAD_INFO =
                    "SELECT nickname, single_score, win, lose "
                    "FROM users "
                    "WHERE login_id=? "
                    "LIMIT 1";

                caches.stmt_cache[DBOperationType::LOAD_SESSION_INFO]
                    .reset(caches.conn->prepareStatement(SQL_LOAD_INFO));

                info_stmt = caches.GetStmt(DBOperationType::LOAD_SESSION_INFO);
                if (!info_stmt)
                {
                    delete info;
                    db_over->ok = false;
                }
            }

            if (db_over->ok)
            {
                // 3) 바인딩
                info_stmt->setString(1, login_id);

                // 4) 실행
                std::unique_ptr<sql::ResultSet> info_rs(info_stmt->executeQuery());

                // 5) 결과 파싱
                if (info_rs && info_rs->next()) // 가져온 결과(행)이 있는지 판별
                {
                    const std::string nickname = info_rs->getString(1);
                    info->single_score = info_rs->getInt(2);
                    info->win_count = info_rs->getInt(3);
                    info->lose_count = info_rs->getInt(4);
                    
                    int copy_len = std::min(nickname.size(), sizeof(info->user_name));
                    ZeroMemory(info->user_name, sizeof(info->user_name)); // 메모리 0으로 초기화(NULL), 파이썬 클라이언트에서 고정 문자열은 뒤에 null을 제거하고 사용, null을 안채우면 그 값도 사용해 이상해짐
                    std::memcpy(info->user_name, nickname.data(), copy_len);

                    // 6) DBOverlapped에 결과 연결
                    db_over->info = info;
                }
                else
                {
                    delete info;
                    db_over->ok = false;
                }
            }
        }
        catch (const sql::SQLException& e)
        {
            PrintErrorLog(__func__, e);
            delete db_over->info;
            db_over->info = nullptr;
            db_over->ok = false;
        }
    }

    PostQueuedCompletionStatus(iocp_handle, DB, session_index, reinterpret_cast<WSAOVERLAPPED*>(db_over));
}

void Database::ExecuteUpdateScore(int session_id, int session_index, const std::string& login_id, int32_t new_score)
{
    auto* db_over = new DBOverlapped{};
    db_over->ex_over.op_type = DB;
    db_over->type = DBOperationType::SCORE_UPDATE;
    db_over->ex_over.operation_id = session_id;
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
                PostQueuedCompletionStatus(iocp_handle, DB, session_index, reinterpret_cast<WSAOVERLAPPED*>(db_over)); // 전송 바이트느 0만 아니면 됨. 어차피 DB 처리는 전송 바이트 처리 필요 없음
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

    PostQueuedCompletionStatus(iocp_handle, DB, session_index, reinterpret_cast<WSAOVERLAPPED*>(db_over));
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
