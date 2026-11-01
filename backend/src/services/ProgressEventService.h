#pragma once
#include "services/TransferService.h"
namespace campusfix {
struct ProgressEvent {std::int64_t id=0,orderId=0,assignmentId=0,orderVersion=0;std::string type,orderStatus,createdAt;};
class ProgressEventService {
public:
    explicit ProgressEventService(SqliteConnection &db,TransferService::Clock clock=[] {return std::chrono::system_clock::now();}):db_(db),clock_(std::move(clock)){}
    ProgressEvent append(std::int64_t user,const std::string &token,std::int64_t order,std::int64_t version,const std::string &type,const std::optional<std::string> &note,const std::string &rid);
private:SqliteConnection &db_;TransferService::Clock clock_;
};
}
