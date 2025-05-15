#pragma once
//#include "Session.h"
#include<array>
#include"define.h"

class Session; // 이미 Session에서 이 파일의 헤더 interface.h를 가지고 있어서 헤더를 선언하면 순환 문제가 생긴다.
class ITestManager;

class IPacketHandler { // 패킷 처리를 위한 추상 클래스

public:
	virtual void ProcessPacket(char* packet) = 0;
	virtual ITestManager* GetManagerInterface() const = 0;
};

class ITestManager { // 서버의 세션 리스트 접근을 위한 추상 클래스
public:
	virtual std::array<std::unique_ptr<Session>, MAX_USER>& GetSessionList() = 0;
	virtual long long GetCurrentTimeMS() = 0;
	virtual void AdjustClientNumber(long long now_time, S2C_TEST_PACKET* p) = 0;
	virtual void Disconnect(int client_id) = 0;
};