#pragma once

#include "services/AuthService.h"
#include <drogon/drogon.h>
#include <filesystem>
#include <functional>
#include <memory>
#include <string>

namespace campusfix {
// Only server-verified, non-secret fields may enter a request identity.
struct RequestIdentity {
    std::int64_t id=0;
    std::string username, displayName, role;
    std::vector<Grant> grants;
    std::int64_t workerId=0;
    std::string requestId;
};
using HttpCallback=std::function<void(const drogon::HttpResponsePtr &)>;
using GuardedHandler=std::function<void(const drogon::HttpRequestPtr &,HttpCallback &&)>;
std::string bearerToken(const drogon::HttpRequestPtr &request);
std::shared_ptr<const RequestIdentity> requestIdentity(const drogon::HttpRequestPtr &request);
// Default preserves existing routes; dispatch opts in to its explicit retryable
// database-error contract without duplicating session verification.
GuardedHandler authenticated(const std::filesystem::path &databasePath,GuardedHandler handler,
                             bool databaseBusyAsUnavailable=false);
}
