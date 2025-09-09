#pragma once
constexpr char S2C_LOGIN = 1;
constexpr char C2S_LOGIN = 2;
constexpr char S2C_MESSAGE = 3;
constexpr char C2S_MESSAGE = 4;
constexpr char S2C_TEST = 5;
constexpr char C2S_TEST = 6;
constexpr char S2C_DISCONNECT = 7;
constexpr char C2S_DISCONNECT = 8;

constexpr char C2S_ADD_OPEN_ROOM = 9;
constexpr char S2C_ADD_OPEN_ROOM = 10;

constexpr char C2S_ADD_LOCK_ROOM = 11;
constexpr char S2C_ADD_LOCK_ROOM = 12;

constexpr char C2S_ADD_USER = 13;
constexpr char S2C_ADD_USER = 14;

constexpr char C2S_DELETE_USER = 15;
constexpr char S2C_DELETE_USER = 16;

constexpr char C2S_READY = 17;
constexpr char S2C_READY = 18;

constexpr char C2S_START = 19;
constexpr char S2C_START = 20;

constexpr char C2S_KICK = 21;
constexpr char S2C_KICK = 22;