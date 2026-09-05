// DBThread.cpp
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cctype>
#include <iostream>
#include <stdexcept>
#include <thread>
#include <jdbc/cppconn/connection.h>
#include <jdbc/cppconn/driver.h>
#include "DBThread.h"
#include "DBResult.h"

#undef min

// -------------------- 유틸(로컬) --------------------
static inline void Trim(std::string& s)
{
    auto not_space = [](unsigned char ch) { return !std::isspace(ch); };

    s.erase(s.begin(), std::find_if(s.begin(), s.end(), not_space));
    s.erase(std::find_if(s.rbegin(), s.rend(), not_space).base(), s.end());
}

// -------------------- DBThread --------------------

DBThread::DBThread()
{
    LoadDBConfigFromFile("db_config.txt");
}

DBThread::~DBThread()
{
}

void DBThread::Init(HANDLE iocp_handle, sql::mysql::MySQL_Driver* driver)
{
    iocp_handle_ = iocp_handle;
    driver_ = driver;
}

void DBThread::Close()
{
    {
        std::lock_guard<std::mutex> lock(wait_mutex_);
        is_running_ = false;
    }
    cv_.notify_one();
}

void DBThread::Wake()
{
    cv_.notify_one();
}

bool DBThread::TryEnqueue(std::unique_ptr<DBTask>& db_task)
{
    try {
        std::lock_guard<std::mutex> lock(wait_mutex_);
        if (!is_running_.load()) return false;
        task_queue_.push(std::move(db_task));
    }
    catch (...) {
        return false;
    }
    cv_.notify_one();
    return true;
}

bool DBThread::Enqueue(std::unique_ptr<ServerDBTask> db_task)
{
    std::unique_ptr<DBTask> task = std::move(db_task);
    while (is_running_.load()) {
        if (TryEnqueue(task)) return true;
        std::this_thread::yield();
    }
    return false;
}

bool DBThread::Enqueue(std::unique_ptr<SessionDBTask> db_task)
{
    std::unique_ptr<DBTask> task = std::move(db_task);
    while (is_running_.load()) {
        if (TryEnqueue(task)) return true;
        std::this_thread::yield();
    }
    return false;
}

void DBThread::Enqueue(std::unique_ptr<MultiSessionDBTask> db_task)
{
    std::unique_ptr<DBTask> task = std::move(db_task);
    while (is_running_.load()) {
        if (TryEnqueue(task)) return;
        std::this_thread::yield();
    }
}

bool DBThread::LoadDBConfigFromFile(const std::string& file_path)
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
            connection_info_.host = val;
        else if (key == "port")
        {
            try
            {
                int p = std::stoi(val);
                if (p < 0) p = 0;
                if (p > 65535) p = 65535;
                connection_info_.port = static_cast<uint16_t>(p);
            }
            catch (...)
            {
                connection_info_.port = 0;
                PrintErrorLog(__func__);
            }
        }
        else if (key == "user")
            connection_info_.id = val;
        else if (key == "password")
            connection_info_.password = val;
        else if (key == "schema")
            connection_info_.schema = val;
    }

    return true;
}

void DBThread::PostDBFailure(const DBTask& task)
{
    if (task.task_target == DBTaskTarget::MULTI_SESSION)
    {
        const auto& multi_session_task = static_cast<const MultiSessionDBTask&>(task);
        for (int i = 0; i < multi_session_task.player_count; ++i)
        {
            auto* db_over = new DBOverlapped{ task.operation_type };
            db_over->ex_over.op_type = OPType::DB;
            db_over->ex_over.session_key = multi_session_task.player_keys[i];
            PostQueuedCompletionStatus(iocp_handle_, static_cast<int>(OPType::DB), DB_SESSION_COMPLETION, reinterpret_cast<WSAOVERLAPPED*>(db_over));
        }
        return;
    }

    auto* db_over = new DBOverlapped{ task.operation_type };

    db_over->ex_over.op_type = OPType::DB;
    if (task.task_target == DBTaskTarget::SESSION) db_over->ex_over.session_key = static_cast<const SessionDBTask&>(task).session_key;
    const ULONG_PTR completion_key = task.task_target == DBTaskTarget::SERVER ? DB_SERVER_COMPLETION : DB_SESSION_COMPLETION;
    PostQueuedCompletionStatus(iocp_handle_, static_cast<int>(OPType::DB), completion_key, reinterpret_cast<WSAOVERLAPPED*>(db_over));
}

// ---- DB 스레드 루프 ----
void DBThread::Run()
{
    if (!Connect())
    {
        std::cerr << "[DB] Failed to connect to database." << std::endl;
        {
            std::lock_guard<std::mutex> lock(wait_mutex_);
            is_running_.store(false);
        }
        std::unique_ptr<DBTask> failed_task;
        while (task_queue_.try_pop(failed_task))
        {
            PostDBFailure(*failed_task);
        }
        return;
    }

    while (is_running_.load())
    {
        std::unique_ptr<DBTask> queued_task;
        if (!task_queue_.try_pop(queued_task))
        {
            std::unique_lock<std::mutex> lock(wait_mutex_);
            // 종료 알림에도 대기에서 빠져나오도록 실행 상태를 조건에 포함한다.
            cv_.wait(lock, [this]() { return !task_queue_.empty() || !is_running_.load(); });

            if (!is_running_.load()) break;

            if (!task_queue_.try_pop(queued_task)) continue;
        }

        // 2) 작업 실행 (DB 실행은 락 없이)
        try {
            ProcessTask(*queued_task);
        }
        catch (const sql::SQLException&) {
            while (is_running_.load() && !TryEnqueue(queued_task)) std::this_thread::yield();
        }
        catch (const std::exception& e) {
            PrintErrorLog(__func__, e);
            PostDBFailure(*queued_task);
        }
        catch (...) {
            PrintErrorLog(__func__);
            PostDBFailure(*queued_task);
        }
    }

    std::unique_ptr<DBTask> discarded_task;
    while (task_queue_.try_pop(discarded_task)) {}

    // 3) 정리
    Disconnect();
}

bool DBThread::Connect()
{
    try
    {
        // Connector/C++ legacy는 보통 tcp://host:port
        const std::string url = "tcp://" + connection_info_.host + ":" + std::to_string(connection_info_.port);

		connection_context_.connection.reset(driver_->connect(url, connection_info_.id, connection_info_.password));
		connection_context_.connection->setSchema(connection_info_.schema);

        return true;
    }
    catch (const sql::SQLException& e)
    {
        PrintErrorLog(__func__, e);
        return false;
    }
}

void DBThread::Disconnect()
{
	connection_context_.statement_cache.clear();

	if (connection_context_.connection)
    {
		try { connection_context_.connection->close(); }
        catch (...) {}
		connection_context_.connection.reset();
    }
}

void DBThread::PrintErrorLog(const char* func_name, const std::exception& e)
{
    std::cerr << "[ERROR] in " << func_name << "\n"
        << "  type: " << typeid(e).name() << "\n"
        << "  what(): " << e.what() << std::endl;
}

void DBThread::PrintErrorLog(const char* func_name, const sql::SQLException& e)
{
    std::cerr << "[DB][SQLException] in " << func_name << "\n"
        << "  what(): " << e.what() << "\n"
        << "  errorCode: " << e.getErrorCode() << "\n"
        << "  SQLState: " << e.getSQLState() << std::endl;
}

void DBThread::PrintErrorLog(const char* func_name)
{
    std::cerr << "[ERROR] in " << func_name
        << "  (unknown exception)" << std::endl;
}
