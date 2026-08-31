#pragma once
#include <condition_variable>
#include <mutex>
#include "ServerThread.h"

class ServerThreadManager;

class TimerThread final : public ServerThread
{
    ServerThreadManager& manager;
    std::mutex wait_mutex;
    std::condition_variable cv;

public:
    TimerThread(ServerThreadManager& manager);
    void Run() override;
    void Close() override;
};
