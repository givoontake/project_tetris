#include <vector>
#include <thread>
#include "TestManager.h"

TestManager test_manager;

void RecvWorkerThread()
{
	while (true) {
		test_manager.ProcessGQCS();
	}
}

void SendWorkerThread()
{
	while (true) {
		test_manager.ProcessSend();
	}
}

int main()
{
	test_manager.SetTestMessege(MAX_MESSAGE_SIZE);
	bool res = test_manager.ConnectToServer();
	if (!res) {
		return 0;
	}
	std::vector <std::thread> worker_threads;
	int recv_worker_threads = std::thread::hardware_concurrency() / 4;
	int send_worker_threads = std::thread::hardware_concurrency() / 4;
	for (int i = 0; i < recv_worker_threads; ++i)
		worker_threads.emplace_back(RecvWorkerThread);

	for (int i = 0; i < send_worker_threads; ++i)
		worker_threads.emplace_back(SendWorkerThread);

	for (auto& th : worker_threads)
		th.join();
}

