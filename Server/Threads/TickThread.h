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

    ServerThreadManager& manager_;
    std::atomic<ThreadState> state_{ ThreadState::COMPLETE };
    std::atomic<TickPhase> phase_{ TickPhase::NONE };
    std::shared_ptr<TickPhaseContext> phase_context_;

    void WaitTickPhase();

public:
    TickThread(ServerThreadManager& manager);
    void Run() override;
    void Close() override;
};
