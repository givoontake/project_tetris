#include <vector>
#include <thread>
#include "IOCPServer.h"

IOCPServer iocp_server;

void worker_thread()
{
	iocp_server.ProcessGQCS();
}

int main()
{
	iocp_server.StartServer();

	std::vector <std::thread> worker_threads;
	int num_threads = std::thread::hardware_concurrency();
	for (int i = 0; i < num_threads; ++i)
		worker_threads.emplace_back(worker_thread);
	for (auto& th : worker_threads)
		th.join();
}

