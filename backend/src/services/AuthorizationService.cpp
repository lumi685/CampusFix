#include "services/AuthorizationService.h"

namespace campusfix {
AccessDecision AuthorizationService::order(std::int64_t id,Action a,std::int64_t orderId) {
    const auto s=repository_.subject(id);
    if(!s.active) return {401,"AUTH_INVALID"};
    return AuthorizationPolicy::order(s,a,repository_.orderScope(orderId,id));
}
AccessDecision AuthorizationService::operation(std::int64_t id,Action a,std::int64_t orderId,std::int64_t version) {
    const auto s=repository_.subject(id);
    if(!s.active) return {401,"AUTH_INVALID"};
    return AuthorizationPolicy::operation(s,a,repository_.orderScope(orderId,id),version);
}
AccessDecision AuthorizationService::location(std::int64_t id,Action a,std::int64_t locationId) {
    const auto s=repository_.subject(id);
    if(!s.active) return {401,"AUTH_INVALID"};
    return AuthorizationPolicy::location(s,a,repository_.ancestors(locationId));
}
AccessDecision AuthorizationService::worker(std::int64_t id,Action a,std::int64_t workerId) {
    const auto s=repository_.subject(id);
    if(!s.active) return {401,"AUTH_INVALID"};
    const auto owner=repository_.workerOwnerAndLocation(workerId);
    if(!owner.first) return {404,"RESOURCE_NOT_FOUND"};
    return AuthorizationPolicy::location(s,a,repository_.ancestors(owner.second),id==owner.first);
}
FieldVisibility AuthorizationService::fields(std::int64_t id,std::int64_t orderId) {
    const auto s=repository_.subject(id);
    if(!s.active) return {};
    const auto o=repository_.orderScope(orderId,id);
    if(!AuthorizationPolicy::mayRead(s,o)) return {};
    const bool manager=(s.role=="dispatcher"||s.role=="sysadmin");
    return {true,AuthorizationPolicy::originalReporter(s,o)||(s.role=="worker"&&o.currentWorker)||manager,
            AuthorizationPolicy::order(s,Action::InternalNotes,o).allowed()};
}
}
