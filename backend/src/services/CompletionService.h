#pragma once
#include "services/TransferService.h"
namespace campusfix {
struct CompletionResult {std::int64_t orderId=0,version=0;std::string status,summary;};
class CompletionService {
public:
    explicit CompletionService(SqliteConnection &db,TransferService::Clock clock=[] {return std::chrono::system_clock::now();}):db_(db),clock_(std::move(clock)){}
    CompletionResult complete(std::int64_t user,const std::string &token,std::int64_t order,std::int64_t version,const std::string &summary,const std::string &rid);
private: SqliteConnection &db_;TransferService::Clock clock_;
};
}
