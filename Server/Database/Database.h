// Database.h
#pragma once
#include <WinSock2.h>
#include <MSWSock.h>

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <exception>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <oneapi/tbb/concurrent_queue.h>

// MySQL Connector/C++ (legacy)
#include <jdbc/mysql_driver.h>
#include <jdbc/mysql_connection.h>
#include <jdbc/cppconn/prepared_statement.h>
#include <jdbc/cppconn/resultset.h>
#include <jdbc/cppconn/exception.h>

#include "ExOverlapped.h"
#include "Types.h"
#include "enum_class.h"
#include "define_packets.h"
#include "DBResult.h"
#include "Session.h"
#include "DBTasks.h"
#include "Threads/ServerThread.h"

struct DBConnectionInfo
{
    std::string host;
    uint16_t port{ 0 };
    std::string id;
    std::string password;
    std::string schema{ "tetris" };
};

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

class Database : public ServerThread
{
protected:
    HANDLE iocp_handle = nullptr;
    DBCaches caches;

public:
    Database();
    ~Database() override;

    Database(const Database&) = delete;
    Database& operator=(const Database&) = delete;

    void Init(HANDLE iocp);
    void Close() override;
    void Wake();
    void Run() override;

    bool Enqueue(std::unique_ptr<ServerDBTask> db_task);
    bool Enqueue(std::unique_ptr<SessionDBTask> db_task, const SP<Session>& session);
    void Enqueue(std::unique_ptr<MultiSessionDBTask> db_task);

protected:
    virtual void ProcessTask(DBTask& task) = 0;
    void PostDBFailure(const DBTask& task);
    static void PrintErrorLog(const char* func_name, const std::exception& e);
    static void PrintErrorLog(const char* func_name, const sql::SQLException& e);
    static void PrintErrorLog(const char* func_name);

private:
    DBConnectionInfo connection_info;

    std::mutex wait_mutex;
    std::condition_variable cv;
    oneapi::tbb::concurrent_queue<std::unique_ptr<DBTask>> task_queue;

    bool TryEnqueue(std::unique_ptr<DBTask>& db_task);
    bool LoadDBConfigFromFile(const std::string& file_path);
    bool Connect();
    void Disconnect();
};
