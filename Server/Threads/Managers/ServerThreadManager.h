#pragma once
#include "DBThreadManager.h"
#include "GameThreadManager.h"
#include "IOThreadManager.h"
#include "LobbyThreadManager.h"
#include "TimerThreadManager.h"
#include "game_state.h"

class IOCPServer;

class ServerThreadManager
{
    IOCPServer& iocp_server_;
	IOThreadManager io_thread_manager_;
	LobbyThreadManager lobby_thread_manager_;
    GameThreadManager game_thread_manager_;
    DBThreadManager db_thread_manager_;
    TimerThreadManager timer_thread_manager_;

    friend class TimerThread;

public:
    ServerThreadManager(IOCPServer& iocp_server, TickWaitPolicy tick_policy = TickWaitPolicy::FULL_SPIN);
    ~ServerThreadManager();

    void StartThreads();
    void CloseThreads();
    void JoinThreads();
};
