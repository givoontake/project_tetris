#pragma once

enum class ThreadState { COMPLETE, PROCESSING };
enum class TickPhase { NONE, ROOM_PROCESS };
enum class TickWaitPolicy { FULL_SPIN, HYBRID_SPIN, FULL_SLEEP };
