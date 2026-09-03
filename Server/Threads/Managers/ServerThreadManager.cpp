#include <thread>
#include "ServerThreadManager.h"
#include "IOCPServer.h"

ServerThreadManager::ServerThreadManager(IOCPServer& iocp_server, TickWaitPolicy tick_policy)
    : iocp_server_(iocp_server), io_thread_manager_(iocp_server), tick_thread_manager_(iocp_server, tick_policy), db_thread_manager_(iocp_server), timer_thread_manager_(*this)
{
    iocp_server_.db_thread_manager_ = &db_thread_manager_;
}

ServerThreadManager::~ServerThreadManager()
{
    iocp_server_.db_thread_manager_ = nullptr;
}

void ServerThreadManager::StartThreads()
{
    const int io_thread_count = static_cast<int>(std::thread::hardware_concurrency()) - TickThreadManager::THREAD_COUNT - TimerThreadManager::THREAD_COUNT - DBThreadManager::THREAD_COUNT;

    tick_thread_manager_.Start();
    timer_thread_manager_.Start();
    db_thread_manager_.Start();
    iocp_server_.RequestLoadRankings();
    io_thread_manager_.Start(io_thread_count);
} 

void ServerThreadManager::CloseThreads()
{
    io_thread_manager_.Close();
    tick_thread_manager_.Close();
    timer_thread_manager_.Close();
    db_thread_manager_.Close();
}

void ServerThreadManager::JoinThreads()
{
    io_thread_manager_.Join();
    tick_thread_manager_.Join();
    timer_thread_manager_.Join();
    db_thread_manager_.Join();
}
