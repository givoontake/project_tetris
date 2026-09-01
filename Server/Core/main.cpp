#include <memory>
#include "IOCPServer.h"
#include "Threads/ServerThreadManager.h"

int main()
{
    SetConsoleOutputCP(65001);

    auto iocp_server = std::make_unique<IOCPServer>();
    iocp_server->StartServer();
    iocp_server->InitDBWorkers();

    ServerThreadManager server_thread_manager(*iocp_server);
    server_thread_manager.StartThreads();
    server_thread_manager.JoinThreads();

    return 0;
}
