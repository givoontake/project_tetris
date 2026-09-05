#pragma once
#include "DBThread.h"

class LoginDBThread final : public DBThread
{
protected:
    void ProcessTask(DBTask& task) override;

private:
    void ExecuteLogin(SessionKey session_key, const std::string& login_id, const std::string& password);
};
