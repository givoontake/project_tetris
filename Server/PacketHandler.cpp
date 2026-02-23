//#include <iostream>
//#include "PacketHandler.h"
//#include "define.h"
//#include "packetType.h"
//#include "IOCPServer.h"
//
////PacketHandler::PacketHandler(IOCPServer* server) : server(server)
////{
////}
////
////// 패킷 핸들러를 따로 만들경우 IOCPServer 맴버 변수 접근을 위한 getter가 많이 필요하다..
////// IServer 가상함수로 만들고 업캐스팅을 하는 작업은..불필요하게 복잡해지는 느낌이 있다.
////// IOCP의 맴버 함수로 만들면 편하긴 한데.. switch로 만들꺼라 너무 길어길 것 같아 걱정이다.. 어떻게 해야할까?
////
//////char temp_id[MAX_USER_ID] = "master";
//////char temp_password[MAX_USER_PASSWORD] = "1234";
//////char temp_name[MAX_USER_NAME] = "master";
////
////void PacketHandler::HandlePacket(char* packet, Session* request_session)
////{
////
////	
////}
//
