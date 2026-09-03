#include <memory>
#include "IOCPServer.h"
#include "Threads/Managers/ServerThreadManager.h"

HANDLE console_close_event = nullptr;

BOOL WINAPI HandleConsoleClose(DWORD signal)
{
    switch (signal) {
    case CTRL_C_EVENT:
    case CTRL_BREAK_EVENT:
    case CTRL_CLOSE_EVENT:
    case CTRL_LOGOFF_EVENT:
    case CTRL_SHUTDOWN_EVENT:
        if (console_close_event) SetEvent(console_close_event);
        return TRUE;

    default:
        return FALSE;
    }
}

int main()
{
    SetConsoleOutputCP(65001);

    console_close_event = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    if (!console_close_event) return 1;
    if (!SetConsoleCtrlHandler(HandleConsoleClose, TRUE)) {
        CloseHandle(console_close_event);
        console_close_event = nullptr;
        return 1;
    }

    auto iocp_server = std::make_unique<IOCPServer>();
    iocp_server->StartServer();

    ServerThreadManager server_thread_manager(*iocp_server);
    server_thread_manager.StartThreads();

    while (iocp_server->IsRunning()) {
        const DWORD wait_result = WaitForSingleObject(console_close_event, 100);
        if (wait_result == WAIT_OBJECT_0 || wait_result == WAIT_FAILED) break;
    }

    server_thread_manager.CloseThreads();
    server_thread_manager.JoinThreads();

    SetConsoleCtrlHandler(HandleConsoleClose, FALSE);
    CloseHandle(console_close_event);
    console_close_event = nullptr;

    return 0;
}
