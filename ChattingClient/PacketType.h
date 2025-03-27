#pragma once
#include <string>
#include <array>
#include <variant>
#include "json.hpp"
#include "protocol.h"

using json = nlohmann::json;

constexpr int MAX_ARRAY_SIZE = 127;

constexpr char S2C_LOGIN = 1;
constexpr char C2S_LOGIN = 2;
constexpr char S2C_MESSAGE = 3;
constexpr char C2S_MESSAGE = 4;

std::string s2c_login = "s2c_login";
std::string c2s_login = "c2s_login";
std::string s2c_message = "s2c_message";
std::string c2s_message = "c2s_message";

class PacketType {
public:
	std::array<char, MAX_ARRAY_SIZE> cPacketTypes;
	std::array<std::string, MAX_ARRAY_SIZE> jPacketTypes;

	PacketType();

	std::string CharToString(char type);
	char StringToChar(std::string type);
};