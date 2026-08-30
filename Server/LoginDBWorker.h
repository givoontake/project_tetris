#pragma once
#include "Database.h"

class LoginDBWorker final : public Database
{
protected:
    void ProcessTask(DBTask& task) override;

private:
    void ExecuteLogin(SessionKey key, const std::string& login_id, const std::string& password);
};
