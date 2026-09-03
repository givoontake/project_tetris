#pragma once
#include <array>
#include <atomic>
#include <cstddef>
#include <memory>
#include <thread>
#include <vector>
#include <jdbc/mysql_driver.h>
#include "DBTasks.h"
#include "GameDBThread.h"
#include "LoginDBThread.h"
#include "Session.h"

class IOCPServer;

class DBThreadManager
{
public:
    static constexpr int GAME_THREAD_COUNT = 2;
    static constexpr int LOGIN_THREAD_COUNT = 1;
    static constexpr int THREAD_COUNT = GAME_THREAD_COUNT + LOGIN_THREAD_COUNT;

private:
    IOCPServer& iocp_server_;
    sql::mysql::MySQL_Driver* driver_ = nullptr;
    LoginDBThread login_thread_;
    std::array<GameDBThread, GAME_THREAD_COUNT> game_threads_;
    std::atomic<std::size_t> next_game_thread_{ 0 };
    std::vector<std::thread> threads_;

public:
    DBThreadManager(IOCPServer& iocp_server);

    void Start();
    void Close();
    void Join();
    void Wake();
    bool Enqueue(std::unique_ptr<ServerDBTask> db_task);
    bool Enqueue(std::unique_ptr<SessionDBTask> db_task, const SP<Session>& session);
    void Enqueue(std::unique_ptr<MultiSessionDBTask> db_task, const SP<Session> (&sessions)[MAX_MATCH_RESULT_PLAYERS]);
};
