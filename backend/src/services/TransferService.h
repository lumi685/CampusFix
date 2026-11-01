#pragma once
#include "repositories/TransferRepository.h"
#include <chrono>
#include <functional>
namespace campusfix {
struct TransferCreated {std::int64_t id=0,orderId=0;std::string orderStatus;};
struct TransferDecision {std::int64_t id=0,version=0;std::string decision,orderStatus;bool assignmentEnded=false;};
class TransferService {
public:
    using Clock=std::function<std::chrono::system_clock::time_point()>;
    explicit TransferService(SqliteConnection &db,Clock clock=[] {return std::chrono::system_clock::now();}):db_(db),clock_(std::move(clock)){}
    TransferCreated create(std::int64_t user,const std::string &token,std::int64_t order,std::int64_t version,const std::string &reason,const std::string &rid);
    TransferDecision decide(std::int64_t user,const std::string &token,std::int64_t order,std::int64_t request,std::int64_t version,const std::string &decision,const std::string &reason,const std::string &rid);
private:SqliteConnection &db_;Clock clock_;
};
}
