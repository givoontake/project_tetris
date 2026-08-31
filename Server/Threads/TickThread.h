#pragma once
#include <atomic>
#include <memory>
#include "ServerThread.h"
#include "tick_state.h"

class ServerThreadManager;
class TickPhaseContext;

class TickThread final : public ServerThread
{
    friend class ServerThreadManager;

    ServerThreadManager& manager;
    std::atomic<THREAD_STATE> state{ THREAD_STATE::COMPLETE };
    std::atomic<TICK_PHASE> phase{ TICK_PHASE::NONE };
    std::shared_ptr<TickPhaseContext> phase_context;

    void WaitTickPhase();

public:
    TickThread(ServerThreadManager& manager);
    void Run() override;
    void Close() override;
};
