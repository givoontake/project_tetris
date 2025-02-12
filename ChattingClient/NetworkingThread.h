#pragma once
#include <thread>
#include <vector>
#include "Session.h"
class NetworkingThread
{
private:
	std::vector<std::thread> threads;
	Session session;
public:
	NetworkingThread(Session session);
	~NetworkingThread();

	void StartWorkerThread();
	void WaitingThreadStop();
	void RecvWorker(Session session);
	void SendWorker(Session session);
};

