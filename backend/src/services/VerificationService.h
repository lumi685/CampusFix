#pragma once
#include "services/TransferService.h"
namespace campusfix {
struct VerificationResult {std::int64_t orderId=0,version=0,reworks=0;bool approved=false;std::string status;};
class VerificationService {
public:
    explicit VerificationService(SqliteConnection &db,TransferService::Clock clock=[] {return std::chrono::system_clock::now();}):db_(db),clock_(std::move(clock)){}
    VerificationResult verify(std::int64_t user,const std::string &token,std::int64_t order,std::int64_t version,bool approved,const std::optional<std::string> &reason,const std::string &rid);
private: SqliteConnection &db_;TransferService::Clock clock_;
};
}
