#pragma once
#include "models/StatusLog.h"
#include "infrastructure/SqliteConnection.h"

namespace campusfix {
// Caller owns authorization and transaction. No UPDATE/DELETE or independent COMMIT.
class StatusLogRepository {
public:
    explicit StatusLogRepository(SqliteConnection &db):db_(db){}
    void append(const StatusLogEntry &entry);
    StatusLogPage page(std::int64_t orderId,const TimelineQuery &query,bool includeInternal);
private:
    SqliteConnection &db_;
};
}
