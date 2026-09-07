#include <chrono>
#include <iomanip>
#include <iostream>
#include <memory>
#include <exception>
#include <sstream>
#include <string>
#include <thread>
#include <vector>
#include "TestManager.h"
#include <Windows.h>

int main(int argc, char* argv[])
{
	std::string server_ip;
	if (argc > 1) server_ip = argv[1];
	else {
		std::cout << "server ip: ";
		std::cin >> server_ip;
	}
	int client_count = MAX_USER;
	if (argc > 2) client_count = std::stoi(argv[2]);
	if (client_count <= 0 || client_count > MAX_USER || client_count % ROOM_PLAYER_COUNT != 0) {
		std::cerr << "client count must be a multiple of " << ROOM_PLAYER_COUNT << " between " << ROOM_PLAYER_COUNT << " and " << MAX_USER << std::endl;
		return 1;
	}

	SYSTEMTIME local_time;
	GetLocalTime(&local_time);
	std::ostringstream output_directory_stream;
	output_directory_stream << ".\\local_test_results\\room_send_comparison\\"
		<< client_count << "_players\\"
		<< std::setfill('0') << std::setw(4) << local_time.wYear
		<< std::setw(2) << local_time.wMonth
		<< std::setw(2) << local_time.wDay << '_'
		<< std::setw(2) << local_time.wHour
		<< std::setw(2) << local_time.wMinute
		<< std::setw(2) << local_time.wSecond;
	const std::string output_directory = output_directory_stream.str();
	std::unique_ptr<TestManager> test_manager;
	try {
		test_manager = std::make_unique<TestManager>(server_ip, output_directory);
	}
	catch (const std::exception& e) {
		std::cerr << e.what() << std::endl;
		return 1;
	}

	std::vector<std::thread> threads;
	for (int i = 0; i < IO_THREAD_COUNT; ++i) threads.emplace_back([&test_manager] { while (true) test_manager->ProcessIO(); });
	for (int i = 0; i < CONNECT_THREAD_COUNT; ++i) threads.emplace_back([&test_manager] { test_manager->ProcessConnect(); });
	for (int i = 0; i < SEND_THREAD_COUNT; ++i) threads.emplace_back([&test_manager, i] {
		while (true) {
			test_manager->ProcessSend(i);
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
		}
	});

	test_manager->Start(client_count);
	while (!test_manager->IsMeasurementFinished()) std::this_thread::sleep_for(std::chrono::milliseconds(100));
	ExitProcess(0);
}
