#include <iostream>
#include <type_traits>
#include "PacketType.h"

PacketType::PacketType()
{
	for (int i = 0; i < MAX_ARRAY_SIZE; ++i){
		cPacketTypes[i] = i;
	}

	for (int i = 0; i < MAX_ARRAY_SIZE; ++i) {
		jPacketTypes[i] = "none";
	}
	jPacketTypes[1] = s2c_login;
	jPacketTypes[2] = c2s_login;
	jPacketTypes[3] = s2c_message;
	jPacketTypes[4] = c2s_message;
}

// 한쪽 패킷이 들어오면 그에 맞는 type 다른 자료형을 반환(char, json(string))하는 함수
// 이짓을 왜 하고 있냐면, map 페어 2개 만들기 싫어서 하고 있는 중.
char PacketType::StringToChar(std::string type)
{
	for (int i = 0; i < MAX_ARRAY_SIZE; i++) {
		if (type == jPacketTypes[i]) {
			return static_cast<char>(i);
		}
	}
	std::cerr << "존재하지 않는 json 패킷 타입입니다. 코드를 확인하세요." << std::endl;
	return static_cast<char>(-1);
}

std::string PacketType::CharToString(char type)
{
	int index = static_cast<int>(type);
	if (jPacketTypes[index] != "none") {
		return jPacketTypes[index];
	}
	std::cerr << "존재하지 않는 json 패킷 타입입니다. 코드를 확인하세요." << std::endl;
	return "invalid_type";
}

