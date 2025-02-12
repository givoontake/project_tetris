#include<iostream>
#include "Session.h"
#include "NetworkingThread.h"
int main()
{
	Session session;

	bool result = session.ConnectToServer();
	if (result == false) {
		std::cout << "연결에 실패하였습니다. 프로그램을 종료합니다." << std::endl;
		return 0;
	}

	NetworkingThread nt(session);
	nt.StartWorkerThread();
	nt.WaitingThreadStop();
}