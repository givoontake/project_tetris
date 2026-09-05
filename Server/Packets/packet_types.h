#pragma once
#include <cstdint>
constexpr std::uint8_t S2C_ERROR = 0;
constexpr std::uint8_t S2C_LOGIN = 1;
constexpr std::uint8_t C2S_LOGIN = 2;
constexpr std::uint8_t S2C_MESSAGE = 3;
constexpr std::uint8_t C2S_MESSAGE = 4;
constexpr std::uint8_t S2C_TEST = 5;
constexpr std::uint8_t C2S_TEST = 6;
constexpr std::uint8_t S2C_DISCONNECT = 7;
constexpr std::uint8_t C2S_DISCONNECT = 8;

constexpr std::uint8_t C2S_ADD_PUBLIC_ROOM = 9;
constexpr std::uint8_t S2C_ADD_PUBLIC_ROOM = 10;

constexpr std::uint8_t C2S_ADD_PRIVATE_ROOM = 11;
constexpr std::uint8_t S2C_ADD_PRIVATE_ROOM = 12;

constexpr std::uint8_t C2S_JOIN_PUBLIC_ROOM = 13;
constexpr std::uint8_t S2C_ADD_PLAYER = 14;

constexpr std::uint8_t C2S_REMOVE_PLAYER = 15;
constexpr std::uint8_t S2C_REMOVE_PLAYER = 16;

constexpr std::uint8_t C2S_READY = 17;
constexpr std::uint8_t S2C_READY = 18;

constexpr std::uint8_t C2S_START = 19;
constexpr std::uint8_t S2C_SINGLE_START = 20;

constexpr std::uint8_t C2S_KICK = 21;
constexpr std::uint8_t S2C_KICK = 22;

constexpr std::uint8_t C2S_MOVE = 23;
constexpr std::uint8_t S2C_MOVE = 24;

constexpr std::uint8_t S2C_SPAWN = 25;

constexpr std::uint8_t S2C_GAME_OVER = 27;

constexpr std::uint8_t S2C_GAME_END = 29;

constexpr std::uint8_t S2C_CLEAR_LINE = 31;

constexpr std::uint8_t S2C_FIX = 33;

constexpr std::uint8_t S2C_ADD_LINE = 35;

constexpr std::uint8_t S2C_UPDATE_SCORE = 37;

constexpr std::uint8_t S2C_MULTI_START = 39;

constexpr std::uint8_t S2C_MATCH_RECORD = 41;

constexpr std::uint8_t S2C_UPDATE_HOST = 43;

constexpr std::uint8_t C2S_GIVE_UP = 45;

constexpr std::uint8_t C2S_REQUEST_ROOM_LIST = 47;

constexpr std::uint8_t S2C_ROOM_INFO = 49;
constexpr std::uint8_t C2S_JOIN_PRIVATE_ROOM = 50;
constexpr std::uint8_t C2S_FAST_MATCHING = 51;  

constexpr std::uint8_t C2S_ADD_FRIEND_REQUEST = 52;
constexpr std::uint8_t C2S_DELETE_FRIEND = 53;
constexpr std::uint8_t C2S_ACCEPT_FRIEND = 54;

constexpr std::uint8_t S2C_ADD_FRIEND_REQUEST = 55;
constexpr std::uint8_t S2C_DELETE_FRIEND = 56;
constexpr std::uint8_t S2C_ADD_FRIEND = 57;

constexpr std::uint8_t C2S_REQUEST_LOBBY_PLAYER_LIST = 58;
constexpr std::uint8_t S2C_LOBBY_PLAYER_INFO = 59;

constexpr std::uint8_t C2S_REQUEST_FRIEND_LIST = 60;
constexpr std::uint8_t S2C_FRIEND_INFO = 61;
constexpr std::uint8_t C2S_REQUEST_RANKINGS = 62;
constexpr std::uint8_t S2C_RANKING_INFO = 63;

constexpr std::uint8_t S2C_INFO = 100;
constexpr std::uint8_t S2C_TEST_LOGIN = 101;
constexpr std::uint8_t C2S_TEST_LOGIN = 102;


