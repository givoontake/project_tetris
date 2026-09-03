#pragma once
#include <memory>
#include <thread>
#include <vector>
#include "IOThread.h"

class IOCPServer;

class IOThreadManager
{
    IOCPServer& iocp_server_;
    std::vector<std::unique_ptr<IOThread>> thread_objects_;
    std::vector<std::thread> threads_;

public:
    IOThreadManager(IOCPServer& iocp_server);

    void Start(int thread_count);
    void Close();
    void Join();
};
