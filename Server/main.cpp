#include <vector>
#include <thread>
#include <iostream>
#include "IOCPServer.h"

IOCPServer iocp_server;
std::atomic<int> remainning_send_IOCP;
std::atomic<int> remainning_total_IOCP; // Accept Á¦¿Ü
std::atomic<int> processed_IOCP;
std::atomic<int> user_count;

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
	iocp_server.StartServer();
	remainning_send_IOCP = 0;
	std::vector <std::thread> worker_threads;
	int num_threads = std::thread::hardware_concurrency();
	for (int i = 0; i < num_threads; ++i)
		worker_threads.emplace_back(WorkerThread);
	/*worker_threads.emplace_back(WorkerThread2);*/
	for (auto& th : worker_threads)
		th.join();
}

