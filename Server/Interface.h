#pragma once
//#include "Session.h"
#include<array>
#include<memory>
#include"define.h"
#include "MQueue.h"

//class Session; // 이미 Session에서 이 파일의 헤더 interface.h를 가지고 있어서 헤더를 선언하면 순환 문제가 생긴다.
//class IServer;

//class IPacketHandler { // 패킷 처리를 위한 추상 클래스
//
//public:
//	virtual void HandlePacket(char* packet) = 0;
//	virtual void Disconnect(int user_id) = 0;
//	virtual IServer* GetServerInterface() const = 0;
//};

//class IServer { // 서버 접근을 위한 추상 클래스
//public:
//	//virtual std::array<std::unique_ptr<Session>, MAX_USER>& GetSessionList() = 0;
//	virtual MQueue& GetTaskQueue() = 0;
//	//virtual void Disconnect(int user_id) = 0;
//};