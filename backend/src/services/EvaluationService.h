#pragma once
#include "repositories/EvaluationRepository.h"
#include "services/TransferService.h"
namespace campusfix {
struct CloseResult {std::int64_t orderId=0,version=0;std::string closedAt;};
class EvaluationService {
public:
    explicit EvaluationService(SqliteConnection &db,TransferService::Clock clock=[] {return std::chrono::system_clock::now();}):db_(db),clock_(std::move(clock)){}
    EvaluationRecord create(std::int64_t user,const std::string &token,std::int64_t order,std::int64_t version,std::int64_t score,const std::optional<std::string> &comment,const std::string &rid);
    std::optional<EvaluationRecord> read(std::int64_t user,const std::string &token,std::int64_t order);
    CloseResult close(std::int64_t user,const std::string &token,std::int64_t order,std::int64_t version,const std::optional<std::string> &note,const std::string &rid);
    ClosablePage list(std::int64_t user,const std::string &token,std::int64_t page=1,std::int64_t pageSize=20);
private: SqliteConnection &db_;TransferService::Clock clock_;
};
}
