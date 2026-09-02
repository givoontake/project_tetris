#pragma once
#include <cstdint>
#include <string>
#include <utility>

#include "ExOverlapped.h"
#include "DBResult.h"
#include "enum_class.h"

constexpr int MAX_MATCH_RESULT_PLAYERS = 5;
static_assert(MAX_MATCH_RESULT_PLAYERS <= 8);

enum class DBTaskTarget : uint8_t
{
    SERVER,
    SESSION,
    MULTI_SESSION
};

struct DBTask
{
    const DBOperationType operation_type;
    const DBTaskTarget task_target;

    DBTask(DBOperationType task_type, DBTaskTarget task_target)
        : operation_type(task_type), task_target(task_target) {}

    virtual ~DBTask() = default;
};

struct ServerDBTask : DBTask
{
    explicit ServerDBTask(DBOperationType task_type) : DBTask(task_type, DBTaskTarget::SERVER) {}
};

struct SessionDBTask : DBTask
{
    SessionKey session_key;

    SessionDBTask(DBOperationType task_type, SessionKey session_key)
        : DBTask(task_type, DBTaskTarget::SESSION), session_key(session_key) {}
};

struct MultiSessionDBTask : DBTask
{
    SessionKey player_keys[MAX_MATCH_RESULT_PLAYERS]{};
    uint8_t player_count{ 0 };
    uint8_t completion_mask{ 0 };

    MultiSessionDBTask(DBOperationType task_type, const SessionKey (&match_player_keys)[MAX_MATCH_RESULT_PLAYERS], uint8_t player_count)
        : DBTask(task_type, DBTaskTarget::MULTI_SESSION), player_count(player_count)
    {
        for (int i = 0; i < MAX_MATCH_RESULT_PLAYERS; ++i) player_keys[i] = match_player_keys[i];
    }
};

struct DBLoginTask final : SessionDBTask
{
    std::string login_id;
    std::string password;

    DBLoginTask(SessionKey session_key, std::string login_id, std::string password)
        : SessionDBTask(DBOperationType::LOGIN, session_key), login_id(std::move(login_id)), password(std::move(password))
    {
    }
};

struct DBLoadRankingsTask final : ServerDBTask
{
	DBLoadRankingsTask() : ServerDBTask(DBOperationType::LOAD_RANKINGS) {}
};

struct DBUpdateScoreTask final : SessionDBTask
{
    int new_score;

    DBUpdateScoreTask(SessionKey session_key, int new_score)
        : SessionDBTask(DBOperationType::UPDATE_SCORE, session_key), new_score(new_score)
    {
    }
};

struct DBUpdateMatchResultTask final : MultiSessionDBTask
{
    SessionKey winner_key;

    DBUpdateMatchResultTask(SessionKey match_winner_key, const SessionKey (&match_player_keys)[MAX_MATCH_RESULT_PLAYERS], uint8_t player_count)
        : MultiSessionDBTask(DBOperationType::UPDATE_MATCH_RESULT, match_player_keys, player_count), winner_key(match_winner_key) {}
};

struct DBAddFriendTask final : SessionDBTask
{
    FriendInfo acceptor_info;
    int requester_id;

    DBAddFriendTask(SessionKey session_key, FriendInfo acceptor_info, int requester_id)
        : SessionDBTask(DBOperationType::ADD_FRIEND, session_key), acceptor_info(std::move(acceptor_info)), requester_id(requester_id)
    {
    }
};

struct DBDeleteFriendTask final : SessionDBTask
{
    int target_id;

    DBDeleteFriendTask(SessionKey session_key, int target_id)
        : SessionDBTask(DBOperationType::DELETE_FRIEND, session_key), target_id(target_id)
    {
    }
};

struct DBAddFriendRequestTask final : SessionDBTask
{
    FriendInfo requester_info;
    int receiver_id;

    DBAddFriendRequestTask(SessionKey session_key, FriendInfo requester_info, int receiver_id)
        : SessionDBTask(DBOperationType::ADD_FRIEND_REQUEST, session_key), requester_info(std::move(requester_info)), receiver_id(receiver_id)
    {
    }
};

struct DBLoadFriendListTask final : SessionDBTask
{
    explicit DBLoadFriendListTask(SessionKey session_key) : SessionDBTask(DBOperationType::LOAD_FRIEND_LIST, session_key)
    {
    }
};
