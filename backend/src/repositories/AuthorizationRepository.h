#pragma once
#include "domain/AuthorizationPolicy.h"
#include "infrastructure/SqliteConnection.h"

namespace campusfix {
class AuthorizationRepository {
public:
    explicit AuthorizationRepository(SqliteConnection &db):db_(db){}
    Subject subject(std::int64_t authenticatedUserId);
    OrderScope orderScope(std::int64_t orderId,std::int64_t authenticatedUserId);
    std::vector<std::int64_t> ancestors(std::int64_t locationId);
    std::pair<std::int64_t,std::int64_t> workerOwnerAndLocation(std::int64_t workerId);
private:
    SqliteConnection &db_;
};
}
