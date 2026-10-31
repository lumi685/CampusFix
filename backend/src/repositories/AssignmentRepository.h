#pragma once
#include "infrastructure/SqliteConnection.h"
#include <optional>

namespace campusfix {
class AssignmentRepository {
public:
    explicit AssignmentRepository(SqliteConnection &db):db_(db){}
    bool hasActive(std::int64_t orderId);
    std::int64_t insert(std::int64_t orderId,std::int64_t workerId,std::int64_t actor,
                         const std::optional<std::string> &reason,const std::string &utc);
    void snapshot(std::int64_t orderId,std::int64_t workerId,std::int64_t actor,std::int64_t assignmentId,
                  std::int64_t version,const std::string &weights,const std::string &candidates,
                  const std::string &excluded,const std::string &utc);
    void advance(std::int64_t orderId,std::int64_t expectedVersion,const std::string &target,const std::string &utc);
private: SqliteConnection &db_;
};
}
