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
    const DBOperationType type;
    const DBTaskTarget target;

    DBTask(DBOperationType task_type, DBTaskTarget task_target)
        : type(task_type), target(task_target) {}

    virtual ~DBTask() = default;
};

struct ServerDBTask : DBTask
{
    explicit ServerDBTask(DBOperationType task_type) : DBTask(task_type, DBTaskTarget::SERVER) {}
};

struct SessionDBTask : DBTask
{
    SessionKey key;

    SessionDBTask(DBOperationType task_type, SessionKey session_key)
        : DBTask(task_type, DBTaskTarget::SESSION), key(session_key) {}
};

struct MultiSessionDBTask : DBTask
{
    SessionKey players[MAX_MATCH_RESULT_PLAYERS]{};
    uint8_t player_count{ 0 };
    uint8_t completion_mask{ 0 };

    MultiSessionDBTask(DBOperationType task_type, const SessionKey (&player_keys)[MAX_MATCH_RESULT_PLAYERS], uint8_t count)
        : DBTask(task_type, DBTaskTarget::MULTI_SESSION), player_count(count)
    {
        for (int i = 0; i < MAX_MATCH_RESULT_PLAYERS; ++i) players[i] = player_keys[i];
    }
};

struct DBLoginTask final : SessionDBTask
{
    std::string login_id;
    std::string password;

    DBLoginTask(SessionKey key, std::string id, std::string pw)
        : SessionDBTask(DBOperationType::LOGIN, key), login_id(std::move(id)), password(std::move(pw))
    {
    }
};

struct DBLoadRankingTask final : ServerDBTask
{
    DBLoadRankingTask() : ServerDBTask(DBOperationType::LOAD_RANKING) {}
};

struct DBUpdateScoreTask final : SessionDBTask
{
    int new_score;

    DBUpdateScoreTask(SessionKey key, int score)
        : SessionDBTask(DBOperationType::UPDATE_SCORE, key), new_score(score)
    {
    }
};

struct DBUpdateMatchResultTask final : MultiSessionDBTask
{
    SessionKey winner;

    DBUpdateMatchResultTask(SessionKey winner_key, const SessionKey (&player_keys)[MAX_MATCH_RESULT_PLAYERS], uint8_t count)
        : MultiSessionDBTask(DBOperationType::UPDATE_MATCH_RESULT, player_keys, count), winner(winner_key) {}
};

struct DBAddFriendTask final : SessionDBTask
{
    FriendInfo accepter_info;
    int requester_id;

    DBAddFriendTask(SessionKey key, FriendInfo accepter, int requester)
        : SessionDBTask(DBOperationType::ADD_FRIEND, key), accepter_info(std::move(accepter)), requester_id(requester)
    {
    }
};

struct DBDeleteFriendTask final : SessionDBTask
{
    int target_id;

    DBDeleteFriendTask(SessionKey key, int target)
        : SessionDBTask(DBOperationType::DELETE_FRIEND, key), target_id(target)
    {
    }
};

struct DBAddFriendRequestTask final : SessionDBTask
{
    FriendInfo requester_info;
    int recver_id;

    DBAddFriendRequestTask(SessionKey key, FriendInfo requester, int recver)
        : SessionDBTask(DBOperationType::ADD_FRIEND_REQUEST, key), requester_info(std::move(requester)), recver_id(recver)
    {
    }
};

struct DBLoadFriendListTask final : SessionDBTask
{
    explicit DBLoadFriendListTask(SessionKey key) : SessionDBTask(DBOperationType::LOAD_FRIEND_LIST, key)
    {
    }
};
