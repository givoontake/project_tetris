#include <vector>
#include <thread>
#include <iostream>
#include "IOCPServer.h"

IOCPServer iocp_server;

void WorkerThread()
{
	iocp_server.ProcessGQCS();
}

//void WorkerThread2()
//{
//	while (iocp_server.GetRunning()) {
//		while (!iocp_server.GetTaskQueue().IsEmpty()) {
//			int user_id = iocp_server.GetTaskQueue().DeQ();
//			iocp_server.Disconnect(user_id);
//			std::cout << "WorkerT_hread2 DeQ, User ID: " << user_id << std::endl;
//		}
//		iocp_server.ProcessGQCS();
//	}
//}

int main()
{
	SetConsoleOutputCP(65001);

	iocp_server.StartServer();
	std::vector <std::thread> worker_threads;
	int num_threads = std::thread::hardware_concurrency();
	for (int i = 0; i < num_threads; ++i)
		worker_threads.emplace_back(WorkerThread);
	/*worker_threads.emplace_back(WorkerThread2);*/
	for (auto& th : worker_threads)
		th.join();
}
