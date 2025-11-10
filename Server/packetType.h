#pragma once
#include <iostream>

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

constexpr char S2C_TEST_LOGIN = 101;
constexpr char C2S_TEST_LOGIN = 102;

inline void PrintPacketType(char type)
{
	std::cout << "Packet Type: ";

	switch (type)
	{
	case S2C_LOGIN: std::cout << "S2C_LOGIN"; break;
	case C2S_LOGIN: std::cout << "C2S_LOGIN"; break;
	case S2C_MESSAGE: std::cout << "S2C_MESSAGE"; break;
	case C2S_MESSAGE: std::cout << "C2S_MESSAGE"; break;
	case S2C_TEST: std::cout << "S2C_TEST"; break;
	case C2S_TEST: std::cout << "C2S_TEST"; break;
	case S2C_DISCONNECT: std::cout << "S2C_DISCONNECT"; break;
	case C2S_DISCONNECT: std::cout << "C2S_DISCONNECT"; break;
	case C2S_ADD_OPEN_ROOM: std::cout << "C2S_ADD_OPEN_ROOM"; break;
	case S2C_ADD_OPEN_ROOM: std::cout << "S2C_ADD_OPEN_ROOM"; break;
	case C2S_ADD_LOCK_ROOM: std::cout << "C2S_ADD_LOCK_ROOM"; break;
	case S2C_ADD_LOCK_ROOM: std::cout << "S2C_ADD_LOCK_ROOM"; break;
	case C2S_ADD_USER: std::cout << "C2S_ADD_USER"; break;
	case S2C_ADD_USER: std::cout << "S2C_ADD_USER"; break;
	case C2S_DELETE_USER: std::cout << "C2S_DELETE_USER"; break;
	case S2C_DELETE_USER: std::cout << "S2C_DELETE_USER"; break;
	case C2S_READY: std::cout << "C2S_READY"; break;
	case S2C_READY: std::cout << "S2C_READY"; break;
	case C2S_START: std::cout << "C2S_START"; break;
	case S2C_START: std::cout << "S2C_START"; break;
	case C2S_KICK: std::cout << "C2S_KICK"; break;
	case S2C_KICK: std::cout << "S2C_KICK"; break;
	case S2C_TEST_LOGIN: std::cout << "S2C_TEST_LOGIN"; break;
	case C2S_TEST_LOGIN: std::cout << "C2S_TEST_LOGIN"; break;
	default: std::cout << "UNKNOWN_PACKET_TYPE"; break;
	}

	std::cout << std::endl;
}

