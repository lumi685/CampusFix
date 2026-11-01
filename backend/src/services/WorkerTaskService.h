#pragma once
#include "infrastructure/SqliteConnection.h"
#include <chrono>
#include <functional>
#include <optional>
namespace campusfix {
struct WorkerTaskResult {std::int64_t orderId=0,version=0;std::string status,acceptedAt;bool assignmentEnded=false;};
class WorkerTaskService {
public:
    using Clock=std::function<std::chrono::system_clock::time_point()>;
    explicit WorkerTaskService(SqliteConnection &db,Clock clock=[] {return std::chrono::system_clock::now();}):db_(db),clock_(std::move(clock)){}
    WorkerTaskResult perform(std::int64_t user,const std::string &token,std::int64_t order,std::int64_t expectedVersion,
                             const std::optional<std::string> &refusalReason,const std::string &requestId);
private:SqliteConnection &db_;Clock clock_;
};
}
