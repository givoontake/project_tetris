#include <iostream>
#include <vector>
#include "json.hpp"
#include "PacketType.h"

using json = nlohmann::json;

PacketType pt;

void JsonToCharPacket(const json& j) {
    std::string type = j.at("type").get<std::string>();
    
    char cPacketType = pt.StringToChar(type);

    switch (cPacketType) {

    }
}