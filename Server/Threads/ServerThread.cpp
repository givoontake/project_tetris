#include "ServerThread.h"

ServerThread::~ServerThread() = default;

void ServerThread::Start()
{
    running = true;
}
