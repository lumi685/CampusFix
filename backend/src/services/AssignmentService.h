#pragma once
#include "services/DispatchService.h"
#include <chrono>
#include <functional>
#include <optional>

namespace campusfix {
struct AssignmentInput {
    std::int64_t workerId=0,expectedVersion=0;
    std::optional<std::string> reason;
};
struct AssignmentResult {
    std::int64_t orderId=0,assignmentId=0,workerId=0,version=0;
    std::string workerName,status;
};
class AssignmentService {
public:
    using Clock=std::function<std::chrono::system_clock::time_point()>;
    explicit AssignmentService(SqliteConnection &db,Clock clock=[] {return std::chrono::system_clock::now();})
        :db_(db),clock_(std::move(clock)){}
    AssignmentResult assign(std::int64_t user,const std::string &token,std::int64_t orderId,
                            const AssignmentInput &,const std::string &requestId);
private:
    SqliteConnection &db_;
    Clock clock_;
};
}
