// Database.h
#pragma once
#include <WinSock2.h>
#include <MSWSock.h>

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <mutex>
#include <queue>
#include <string>
#include <thread>
#include <unordered_map>
#include <memory>

// MySQL Connector/C++ (legacy)
#include <jdbc/mysql_driver.h>
#include <jdbc/mysql_connection.h>
#include <jdbc/cppconn/prepared_statement.h>
#include <jdbc/cppconn/resultset.h>
#include <jdbc/cppconn/exception.h>

#include "ExOverlapped.h"
#include "enum_class.h"
#include "define.h"
#include "DBResult.h"
#include "Session.h"

// 반환형은 void, 실행 시 인자를 받지 않는다.
// 실제로는 callable 객체(std::function)이며, 람다(closure object)를 담아 사용한다.
// 람다는 “함수처럼 호출 가능한 객체”이므로 std::function으로 감쌀 수 있고, 값으로 복사되어 큐에 보관된다.
// 캡처는 함수 인자가 아니라 객체의 멤버 상태이며, 객체 복사 시 캡처된 값도 함께 복사된다.
// DBSession은 DB 스레드 내부에서만 관리되며, 작업 실행 시 Database 내부에서 접근한다.
using Task = std::function<void()>; // std::function은 컨테이너가 아니다. 지정된 반환형과 인자와 동일한 함수를 감싸주는 래퍼일 뿐이다. 즉 함수를 객체처럼 사용할 수 있게 해주는 것!

class Database
{
    struct DBCaches
    {
        std::unique_ptr<sql::Connection> conn;
        std::unordered_map<DBOperationType, std::unique_ptr<sql::PreparedStatement>> stmt_cache;

        sql::PreparedStatement* GetStmt(DBOperationType id)
        {
            auto it = stmt_cache.find(id);
            return (it == stmt_cache.end()) ? nullptr : it->second.get();
        }
    };

private:
    HANDLE iocp_handle;

    // ---- DB 설정(생성자에서 파일로부터 읽어 초기화) ----
    std::string db_host;
    uint16_t db_port{ 0 };
    std::string db_id;
    std::string db_password;
    std::string schema;

    std::atomic<bool> running{ false };

    std::mutex mutex;
    std::condition_variable cv;
    std::queue<Task> pending_db_tasks;

    // ---- DB 스레드 전용 컨텍스트 접근용 ----
    // ThreadLoop에서만 세팅/사용 (외부 노출 X)
    DBCaches caches;

public:
    // IOCP 완료 통지에 사용할 completion key (서버에서 이 키로 DB 완료인지 분기)

public:
    // 생성자에서 DB 설정 파일을 읽어 멤버(db_host/db_port/...)를 초기화한다.
   /* explicit Database(HANDLE iocp_handle);*/
    Database();
    ~Database();

    bool GetRunning() const { return running.load(); }
    void SetRunning(bool val) { running.store(val); }

    void init(HANDLE iocp);
    void Run();
    //Database(const Database&) = delete;
    //Database& operator=(const Database&) = delete;

    // ---- 외부에서 작업을 큐에 넣는 API ----
    // 외부에서 람다를 만들어 그대로 큐에 넣는다.
    // (람다 내부에서 실제 쿼리 실행 함수(ExecuteXXX)를 호출하는 방식)
    void Enqueue(Task job);

    // ---- DB 스레드에서 실행될 "실제 DB 작업" 함수들 ----
    // ⚠️ 이 함수들은 "DB 스레드에서만" 호출되어야 한다.
    // 외부는 보통 아래처럼 람다에 넣어 Enqueue 한다:
    //   db.Enqueue([&db, sid, id, pw]{ db.ExecuteLogin(sid, id, pw); });
    void ExecuteLogin(SessionKey key, const std::string login_id, const std::string password);
    void ExecuteLoadRanking();
    void ExecuteUpdateScore(SessionKey key, int user_id, int new_score);
	void ExecuteUpdateMatchResult(SessionKey key, int user_id, bool is_winner);
    void ExecuteAddFriend(int requester_id, const FriendInfo& accepter_info);
	void ExecuteDeleteFriend(int requester_id, int target_id);
    void ExecuteAddFriendRequest(const FriendInfo& requester_info, int recver_id);
	void ExecuteLoadFriendList(SessionKey key, int user_id);

private:
    // ---- 설정 파일 로드 ----
    // 파일에서 host/port/user/pass/schema를 읽어 멤버에 저장
    bool LoadDBConfigFromFile(const std::string& file_path);

private:
    bool Connect();
    void Disconnect();
};
