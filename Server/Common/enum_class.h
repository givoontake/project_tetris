#pragma once
#include "error_code.h"
#include "info_code.h"

enum class OPType { SEND, POOLED_SEND, RECV, ACCEPT, DB };

enum class DBOperationType { LOGIN, LOAD_SESSION_INFO, LOAD_RANKINGS, UPDATE_SCORE, UPDATE_MATCH_RESULT,
							ADD_FRIEND, DELETE_FRIEND, ADD_FRIEND_REQUEST, DELETE_FRIEND_REQUEST, GET_FRIEND_INFO, LOAD_FRIEND_LIST, LOAD_MATCH_RECORD, DB_ERROR };

enum class RoomPlayerState { EMPTY, WAIT, READY, PLAY, GAME_OVER };

enum class RoomState { EMPTY, WAIT, PLAY, WAITING_DELETE, DELETE_POST };

enum class RoomProcessState { COMPLETE, PROCESSING };

enum class ActiveEntryState { EMPTY, ACTIVE, PENDING };

enum class RoomExitType { LEAVE, KICK };

enum class RoomTaskType { NONE, REMOVE_PLAYER, READY, KICK, START, GIVE_UP };

enum class LifeState { NONE, INITIALIZING, ACTIVE, DISCONNECT_PENDING, DISCONNECTING };

enum class ModeState { NONE, LOGIN, LOBBY, ROOM };

enum class EventType { NONE = -1, RIGHT, LEFT, ROTATE, DOWN, DROP, UP, MOVE, FIX, CLEAR_LINE, ADD_LINE, GAME_OVER, GAME_END, SPAWN };
