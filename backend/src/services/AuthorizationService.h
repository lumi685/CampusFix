#pragma once
#include "repositories/AuthorizationRepository.h"

namespace campusfix {
// authenticatedUserId must come from AuthGuard, never request JSON/query parameters.
// For writes, construct this service on the SAME transaction connection used for the mutation.
// This service makes no business writes. Future services still enforce required fields,
// reason/score constraints, version-conditional writes and unique constraints atomically.
class AuthorizationService {
public:
    explicit AuthorizationService(SqliteConnection &db):repository_(db){}
    AccessDecision order(std::int64_t authenticatedUserId,Action action,std::int64_t orderId);
    AccessDecision operation(std::int64_t authenticatedUserId,Action action,std::int64_t orderId,
                             std::int64_t expectedVersion);
    AccessDecision location(std::int64_t authenticatedUserId,Action action,std::int64_t locationId);
    AccessDecision worker(std::int64_t authenticatedUserId,Action action,std::int64_t workerId);
    FieldVisibility fields(std::int64_t authenticatedUserId,std::int64_t orderId);
private:
    AuthorizationRepository repository_;
};
}
