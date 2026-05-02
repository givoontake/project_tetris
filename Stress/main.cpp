#include <iostream>
#include <vector>
#include <thread>
#include <chrono>
#include "TestManager.h"

TestManager test_manager;

void IocpWorkerThread()
{
	while (true) {
		test_manager.ProcessGQCS();
	}
}

void ViewWorkerThread()
{
	test_manager.ProcessViewSocket();
}

void ConnectWorkerThread()
{
	test_manager.ProcessConnectThread();
}

int main()
{
	test_manager.SetTestMessege(MAX_MESSAGE_SIZE);

	std::vector<std::thread> worker_threads;
	for (int i = 0; i < STRESS_WORKER_THREAD_COUNT; ++i) {
		worker_threads.emplace_back(IocpWorkerThread);
	}
	worker_threads.emplace_back(ConnectWorkerThread);
	worker_threads.emplace_back(ViewWorkerThread);

	if (!test_manager.StartConnectSessions(STRESS_SESSION_COUNT)) {
		std::cout << "some stress clients failed to start" << std::endl;
	}

	while (true) {
		test_manager.ProcessSend();
		std::this_thread::sleep_for(std::chrono::milliseconds(10));
	}

	for (auto& th : worker_threads) {
		th.join();
	}
}
