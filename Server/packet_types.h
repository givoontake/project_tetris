#pragma once
#include <cstdint>
#include <iostream>
constexpr std::uint8_t S2C_ERROR = 0;
constexpr std::uint8_t S2C_LOGIN = 1;
constexpr std::uint8_t C2S_LOGIN = 2;
constexpr std::uint8_t S2C_MESSAGE = 3;
constexpr std::uint8_t C2S_MESSAGE = 4;
constexpr std::uint8_t S2C_TEST = 5;
constexpr std::uint8_t C2S_TEST = 6;
constexpr std::uint8_t S2C_DISCONNECT = 7;
constexpr std::uint8_t C2S_DISCONNECT = 8;

constexpr std::uint8_t C2S_ADD_OPEN_ROOM = 9;
constexpr std::uint8_t S2C_ADD_OPEN_ROOM = 10;

constexpr std::uint8_t C2S_ADD_LOCK_ROOM = 11;
constexpr std::uint8_t S2C_ADD_LOCK_ROOM = 12;

constexpr std::uint8_t C2S_JOIN_OPEN_ROOM = 13;
constexpr std::uint8_t S2C_ADD_USER = 14;

constexpr std::uint8_t C2S_DELETE_USER = 15;
constexpr std::uint8_t S2C_DELETE_USER = 16;

constexpr std::uint8_t C2S_READY = 17;
constexpr std::uint8_t S2C_READY = 18;

constexpr std::uint8_t C2S_START = 19;
constexpr std::uint8_t S2C_SINGLE_START = 20;

constexpr std::uint8_t C2S_KICK = 21;
constexpr std::uint8_t S2C_KICK = 22;

constexpr std::uint8_t C2S_MOVE = 23;
constexpr std::uint8_t S2C_MOVE = 24;

constexpr std::uint8_t S2C_SPAWN = 25;

constexpr std::uint8_t S2C_GAMEOVER = 27;

constexpr std::uint8_t S2C_GAMEEND = 29;

constexpr std::uint8_t S2C_CLEARLINE = 31;

constexpr std::uint8_t S2C_FIX = 33;

constexpr std::uint8_t S2C_ADDLINE = 35;

constexpr std::uint8_t S2C_UPDATE_SCORE = 37;

constexpr std::uint8_t S2C_MULTI_START = 39;

constexpr std::uint8_t S2C_MATCH_RECORD = 41;

constexpr std::uint8_t S2C_UPDATE_HOST = 43;

constexpr std::uint8_t C2S_GIVEUP = 45;

constexpr std::uint8_t C2S_REQUEST_ROOM_LIST = 47;

constexpr std::uint8_t S2C_ROOM_INFO = 49;
constexpr std::uint8_t C2S_JOIN_LOCK_ROOM = 50;
constexpr std::uint8_t C2S_FAST_MATCHING = 51;  

constexpr std::uint8_t C2S_REQUEST_FRIEND = 52;
constexpr std::uint8_t C2S_DELETE_FRIEND = 53;
constexpr std::uint8_t C2S_ACCEPT_FRIEND = 54;

constexpr std::uint8_t S2C_REQUEST_FRIEND = 55;
constexpr std::uint8_t S2C_DELETE_FRIEND = 56;
constexpr std::uint8_t S2C_ADD_FRIEND = 57;

constexpr std::uint8_t C2S_REQUEST_LOBBY_USER_LIST = 58;
constexpr std::uint8_t S2C_LOBBY_USER_INFO = 59;

constexpr std::uint8_t C2S_REQUEST_FRIEND_LIST = 60;
constexpr std::uint8_t S2C_FRIEND_INFO = 61;
constexpr std::uint8_t C2S_REQUEST_RANKING = 62;
constexpr std::uint8_t S2C_RANKING_INFO = 63;

constexpr std::uint8_t S2C_INFO = 100;
constexpr std::uint8_t S2C_TEST_LOGIN = 101;
constexpr std::uint8_t C2S_TEST_LOGIN = 102;

inline void PrintPacketType(std::uint8_t type)
{
    switch (type)
    {
    case S2C_ERROR:             std::cout << "S2C_ERROR"; break;

    case S2C_LOGIN:             std::cout << "S2C_LOGIN"; break;
    case C2S_LOGIN:             std::cout << "C2S_LOGIN"; break;

    case S2C_MESSAGE:           std::cout << "S2C_MESSAGE"; break;
    case C2S_MESSAGE:           std::cout << "C2S_MESSAGE"; break;

    case S2C_TEST:              std::cout << "S2C_TEST"; break;
    case C2S_TEST:              std::cout << "C2S_TEST"; break;

    case S2C_DISCONNECT:        std::cout << "S2C_DISCONNECT"; break;
    case C2S_DISCONNECT:        std::cout << "C2S_DISCONNECT"; break;

    case C2S_ADD_OPEN_ROOM:     std::cout << "C2S_ADD_OPEN_ROOM"; break;
    case S2C_ADD_OPEN_ROOM:     std::cout << "S2C_ADD_OPEN_ROOM"; break;

    case C2S_ADD_LOCK_ROOM:     std::cout << "C2S_ADD_LOCK_ROOM"; break;
    case S2C_ADD_LOCK_ROOM:     std::cout << "S2C_ADD_LOCK_ROOM"; break;

    case C2S_JOIN_OPEN_ROOM:    std::cout << "C2S_JOIN_OPEN_ROOM"; break;
    case C2S_JOIN_LOCK_ROOM:    std::cout << "C2S_JOIN_LOCK_ROOM"; break;
    case S2C_ADD_USER:          std::cout << "S2C_ADD_USER"; break;

    case C2S_DELETE_USER:       std::cout << "C2S_DELETE_USER"; break;
    case S2C_DELETE_USER:       std::cout << "S2C_DELETE_USER"; break;

    case C2S_READY:             std::cout << "C2S_READY"; break;
    case S2C_READY:             std::cout << "S2C_READY"; break;

    case C2S_START:             std::cout << "C2S_START"; break;
    case S2C_SINGLE_START:      std::cout << "S2C_SINGLE_START"; break;
    case S2C_MULTI_START:       std::cout << "S2C_MULTI_START"; break;

    case C2S_KICK:              std::cout << "C2S_KICK"; break;
    case S2C_KICK:              std::cout << "S2C_KICK"; break;

    case C2S_MOVE:              std::cout << "C2S_MOVE"; break;
    case S2C_MOVE:              std::cout << "S2C_MOVE"; break;

    case S2C_SPAWN:             std::cout << "S2C_SPAWN"; break;
    case S2C_FIX:               std::cout << "S2C_FIX"; break;
    case S2C_CLEARLINE:         std::cout << "S2C_CLEARLINE"; break;
    case S2C_ADDLINE:           std::cout << "S2C_ADDLINE"; break;
    case S2C_GAMEOVER:          std::cout << "S2C_GAMEOVER"; break;
    case S2C_GAMEEND:           std::cout << "S2C_GAMEEND"; break;

    case S2C_UPDATE_SCORE:      std::cout << "S2C_UPDATE_SCORE"; break;
    case S2C_MATCH_RECORD:      std::cout << "S2C_MATCH_RECORD"; break;
    case S2C_UPDATE_HOST:       std::cout << "S2C_UPDATE_HOST"; break;

    case C2S_GIVEUP:            std::cout << "C2S_GIVEUP"; break;
    case C2S_REQUEST_ROOM_LIST: std::cout << "C2S_REQUEST_ROOM_LIST"; break;
    case S2C_ROOM_INFO:         std::cout << "S2C_ROOM_INFO"; break;

    case S2C_TEST_LOGIN:        std::cout << "S2C_TEST_LOGIN"; break;
    case C2S_TEST_LOGIN:        std::cout << "C2S_TEST_LOGIN"; break;
    case C2S_REQUEST_RANKING:   std::cout << "C2S_REQUEST_RANKING"; break;
    case S2C_RANKING_INFO:      std::cout << "S2C_RANKING_INFO"; break;

    default:                    std::cout << "UNKNOWN_PACKET_TYPE"; break;
    }

    std::cout << " (" << static_cast<int>(type) << ")\n";
}


