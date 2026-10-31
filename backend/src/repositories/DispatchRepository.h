#pragma once

#include "dispatch/DispatchModels.h"
#include "infrastructure/SqliteConnection.h"

namespace campusfix {
struct DispatchData {
    dispatch::DispatchWorkOrder order;
    std::vector<dispatch::DispatchWorker> workers;
};

// The caller owns authentication, authorization and the consistent read/write
// transaction. This adapter performs SELECTs only and never computes scores.
class DispatchRepository {
public:
    explicit DispatchRepository(SqliteConnection &db) : db_(db) {}
    DispatchData load(std::int64_t orderId);
private:
    SqliteConnection &db_;
};
}
