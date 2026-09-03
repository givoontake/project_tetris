#include "DBThreadManager.h"
#include "IOCPServer.h"

static_assert(DBThreadManager::GAME_THREAD_COUNT > 0);
static_assert(DBThreadManager::LOGIN_THREAD_COUNT == 1);

DBThreadManager::DBThreadManager(IOCPServer& iocp_server)
    : iocp_server_(iocp_server), driver_(sql::mysql::get_mysql_driver_instance())
{
}

void DBThreadManager::Start()
{
    login_thread_.Init(iocp_server_.GetIOCPHandle(), driver_);
    for (auto& db_thread : game_threads_)
        db_thread.Init(iocp_server_.GetIOCPHandle(), driver_);

    threads_.reserve(THREAD_COUNT);
    login_thread_.Start();
    threads_.emplace_back(&ServerThread::Run, &login_thread_);
    for (auto& db_thread : game_threads_) {
        db_thread.Start();
        threads_.emplace_back(&ServerThread::Run, &db_thread);
    }
}

void DBThreadManager::Close()
{
    login_thread_.Close();
    for (auto& db_thread : game_threads_)
        db_thread.Close();
}

void DBThreadManager::Join()
{
    for (auto& thread : threads_)
        thread.join();
}

void DBThreadManager::Wake()
{
    login_thread_.Wake();
    for (auto& db_thread : game_threads_)
        db_thread.Wake();
}

bool DBThreadManager::Enqueue(std::unique_ptr<ServerDBTask> db_task)
{
    const std::size_t thread_index = next_game_thread_.fetch_add(1) % game_threads_.size();
    return game_threads_[thread_index].Enqueue(std::move(db_task));
}

bool DBThreadManager::Enqueue(std::unique_ptr<SessionDBTask> db_task, const SP<Session>& session)
{
    bool is_enqueued = false;
    if (db_task->operation_type == DBOperationType::LOGIN) {
        is_enqueued = login_thread_.Enqueue(std::move(db_task), session);
    }
    else {
        std::size_t thread_index = 0;
        if (db_task->session_key.player_id >= 0)
            thread_index = static_cast<std::size_t>(db_task->session_key.player_id) % game_threads_.size();
        else
            thread_index = next_game_thread_.fetch_add(1) % game_threads_.size();

        is_enqueued = game_threads_[thread_index].Enqueue(std::move(db_task), session);
    }

    if (!is_enqueued && session->TryDeactivate()) iocp_server_.TryDisconnect(session);
    return is_enqueued;
}

void DBThreadManager::Enqueue(std::unique_ptr<MultiSessionDBTask> db_task, const SP<Session> (&sessions)[MAX_MATCH_RESULT_PLAYERS])
{
    for (int i = 0; i < db_task->player_count; ++i) {
        if (sessions[i]->TryAddPending()) db_task->completion_mask |= static_cast<uint8_t>(1u << i);
        else if (sessions[i]->TryDeactivate()) iocp_server_.TryDisconnect(sessions[i]);
    }

    const std::size_t thread_index = static_cast<std::size_t>(db_task->player_keys[0].player_id) % game_threads_.size();
    game_threads_[thread_index].Enqueue(std::move(db_task));
}
