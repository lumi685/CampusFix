#pragma once
#include "infrastructure/SqliteConnection.h"
#include <optional>
#include <vector>

namespace campusfix {
struct EvaluationRecord {
    std::int64_t id=0,orderId=0,reporterId=0,score=0;
    std::optional<std::string> comment;
    std::string createdAt;
};
struct ClosableItem {std::int64_t id=0,version=0;std::optional<std::string> orderNo;std::string updatedAt;};
struct ClosablePage {std::vector<ClosableItem> items;std::int64_t page=1,pageSize=20,total=0;};
class EvaluationRepository {
public:
    explicit EvaluationRepository(SqliteConnection &db):db_(db){}
    std::optional<EvaluationRecord> find(std::int64_t order);
    EvaluationRecord insert(std::int64_t order,std::int64_t reporter,std::int64_t score,const std::optional<std::string> &comment,const std::string &utc);
    // Exact business evidence, kept locally for transaction-end invariant checks.
    std::string stableOrder(std::int64_t order,bool closing);
    std::string history(std::int64_t order);
    std::string completionTime(std::int64_t order);
    void close(std::int64_t order,std::int64_t version,const std::string &utc);
    ClosablePage list(std::int64_t user,const std::string &cap,std::int64_t page,std::int64_t pageSize);
private: SqliteConnection &db_;
};
}
