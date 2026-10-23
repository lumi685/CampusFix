#pragma once
#include "infrastructure/SqliteConnection.h"
#include "models/ManagementGrant.h"
#include <cstdint>
#include <string>
#include <vector>

namespace campusfix {
struct AuthUser { std::int64_t id=0; std::string username, passwordHash, displayName, role, status;
                  std::string tokenHash; bool expired=false, revoked=false; };
class AuthRepository {
public:
    explicit AuthRepository(SqliteConnection &db):db_(db){}
    AuthUser userByName(const std::string &username);
    AuthUser userByTokenHash(const std::string &hash);
    std::vector<Grant> grants(std::int64_t userId);
    std::int64_t workerId(std::int64_t userId);
    void createSession(const std::string &id,std::int64_t userId,const std::string &hash,
                       const std::string &expires);
    bool revokeSession(const std::string &hash);
private: SqliteConnection &db_;
};
}
