#pragma once
#include "repositories/WorkerRepository.h"
#include <chrono>
#include <functional>

namespace campusfix {
class WorkerService {
public:
    using Clock=std::function<std::chrono::system_clock::time_point()>;
    explicit WorkerService(SqliteConnection &db,Clock clock=[] {return std::chrono::system_clock::now();}):db_(db),clock_(std::move(clock)){}
    WorkerPage list(std::int64_t user,const std::string &token,const WorkerQuery &query);
    WorkerLoad workload(std::int64_t user,const std::string &token,std::int64_t id);
    WorkerRecord update(std::int64_t user,const std::string &token,std::int64_t id,const WorkerInput &input);
private:
    SqliteConnection &db_;Clock clock_;
    void authenticate(std::int64_t user,const std::string &token);
    void authorize(std::int64_t user,std::int64_t worker,bool write);
};
}
