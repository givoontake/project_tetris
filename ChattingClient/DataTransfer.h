#include <iostream>
#include <vector>
#include "json.hpp"
#include "PacketType.h"

using json = nlohmann::json;

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(S2C_LOGIN_PACKET, size, type, id)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(S2C_MESSAGE_PACKET, size, type, id, message)

PacketType pt;

std::vector<char> JsonToCharPacket(const json& j) {
    std::string type = j.at("type").get<std::string>();
    char cPacketType = pt.StringToChar(type);

    std::vector<char> bytes;

    try {
        switch (cPacketType) {
        case C2S_LOGIN: {
            C2S_LOGIN_PACKET p{};
            p.size = j.at("size").get<char>();
            p.type = cPacketType;

            std::string id = j.at("id").get<std::string>();
            std::strncpy(p.id, id.c_str(), ID_SIZE);

            bytes.resize(sizeof(p));
            std::memcpy(bytes.data(), &p, sizeof(p));
            return bytes;
        }

        case C2S_MESSAGE: {
            C2S_MESSAGE_PACKET p{};
            p.size = j.at("size").get<char>();
            p.type = cPacketType;

            std::string id = j.at("id").get<std::string>();

            std::string message = j.at("message").get<std::string>();
            std::strncpy(p.message, message.c_str(), BUFFER_SIZE);

            bytes.resize(sizeof(p));
            std::memcpy(bytes.data(), &p, sizeof(p));
            return bytes;
        }

        default:
            std::cerr << "[경고] 지원하지 json 않는 패킷 타입입니다. 코드를 확인하세요.\n";
            return {};
        }
    }
    catch (const json::out_of_range& e) {
        std::cerr << "[예외] json 필드 없음: " << e.what() << "\n";
    }
    catch (const json::exception& e) {
        std::cerr << "[예외] json 처리 중 오류: " << e.what() << "\n";
    }

    return {};  // 실패 시 빈 vector 반환
}


json CharToJsonPacket(const char* packet) {
    switch (packet[1]) {
    case S2C_LOGIN: {
        S2C_LOGIN_PACKET p{};
        std::memcpy(&p, packet, sizeof(p));
        return json(p);
    }
    case S2C_MESSAGE: {
        S2C_MESSAGE_PACKET p{};
        std::memcpy(&p, packet, sizeof(p));
        return json(p);
    }
    default:
        std::cerr << "[경고] 알 수 없는 패킷 타입입니다: " << static_cast<int>(packet[1]) << " 코드를 확인하세요.\n";
        return {};
    }
}
