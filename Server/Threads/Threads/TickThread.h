#pragma once
#include <atomic>
#include <memory>
#include "ServerThread.h"
#include "tick_state.h"

class TickThreadManager;
struct TickPhaseContext;

class TickThread final : public ServerThread
{
    friend class TickThreadManager;

    TickThreadManager& manager_;
    std::atomic<TickThreadState> state_{ TickThreadState::AVAILABLE };
    std::atomic<TickPhase> phase_{ TickPhase::NONE };
    std::shared_ptr<TickPhaseContext> phase_context_;

    void WaitTickPhase();

public:
    TickThread(TickThreadManager& manager);
    void Run() override;
    void Close() override;
};
