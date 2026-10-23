#pragma once
#include "repositories/WorkOrderRepository.h"
#include <chrono>
#include <functional>

namespace campusfix {
class WorkOrderService {
public:
    using Clock=std::function<std::chrono::system_clock::time_point()>;
    explicit WorkOrderService(SqliteConnection &db,Clock clock=[] {return std::chrono::system_clock::now();})
        :db_(db),clock_(std::move(clock)){}
    WorkOrder create(std::int64_t authenticatedId,const std::string &token,const WorkOrderInput &input,const std::string &requestId);
    WorkOrder submit(std::int64_t authenticatedId,const std::string &token,std::int64_t id,
                     std::int64_t expectedVersion,const std::string &requestId);
private:
    SqliteConnection &db_;
    Clock clock_;
    void authenticate(std::int64_t id,const std::string &token);
};
}
