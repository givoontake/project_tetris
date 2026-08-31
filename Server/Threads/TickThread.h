#pragma once
#include "ServerThread.h"

class ServerThreadManager;

class TickThread final : public ServerThread
{
    ServerThreadManager& manager;
    int thread_num;

public:
    TickThread(ServerThreadManager& manager, int num);
    void Run() override;
    void Close() override;
};
