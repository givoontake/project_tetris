#include <string>
#include <iostream>
#include "NetworkingThread.h"
#include "protocol.h"
NetworkingThread::NetworkingThread(Session session) : session(session)
{

}

NetworkingThread::~NetworkingThread()
{
}



void NetworkingThread::StartWorkerThread()
{
	// 스레드는 벡터에 추가되는 즉시 실행된다.
	threads.emplace_back(&NetworkingThread::RecvWorker, this, std::ref(session));
	threads.emplace_back(&NetworkingThread::SendWorker, this, std::ref(session));
}

void NetworkingThread::WaitingThreadStop()
{
	for (auto& t : threads) {
		if (t.joinable()) {
			t.join();
		}
	}
	std::cout << "Worker thread 모두 종료." << std::endl;

}

void NetworkingThread::RecvWorker(Session session)
{
	char buffer[BUFFER_SIZE];
	while (true) {
		int recv_bytes = recv(session.GetSocket(), buffer, BUFFER_SIZE, 0);
		session.MergePacket(recv_bytes, buffer);
	}
}

void NetworkingThread::SendWorker(Session session)
{
	std::string message;
	while (true) {
		std::cin >> message;
		session.ProcessSendPacket(message);
	}
}
