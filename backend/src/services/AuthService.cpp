#include "services/AuthService.h"
#include "infrastructure/AuthCrypto.h"
#include <sodium.h>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <stdexcept>

namespace campusfix {
namespace {
std::string utcExpiry() {
    auto when=std::chrono::system_clock::now()+std::chrono::hours(24);
    std::time_t t=std::chrono::system_clock::to_time_t(when);
    std::tm utc{};
#ifdef _WIN32
    gmtime_s(&utc,&t);
#else
    gmtime_r(&t,&utc);
#endif
    std::ostringstream out; out<<std::put_time(&utc,"%Y-%m-%dT%H:%M:%S.000Z"); return out.str();
}
}
LoginResult AuthService::login(const std::string &name,const std::string &password) {
    SqliteConnection db(path_); AuthRepository repo(db);
    auto user=repo.userByName(name);
    // Keep absent/disabled account verification computationally comparable.
    static const std::string dummy=hashPassword("unusable internal dummy credential");
    const bool verified=verifyPassword(password,user.id && user.status=="ACTIVE" ? user.passwordHash:dummy);
    if (!user.id || user.status!="ACTIVE" || !verified) throw std::invalid_argument("AUTH_INVALID");
    LoginResult result{newToken(),utcExpiry(),user};
    SqliteTransaction tx(db);
    repo.createSession(newOpaqueId(),user.id,tokenDigest(result.token),result.expiresAt);
    tx.commit();
    return result;
}
MeResult AuthService::me(const std::string &token) {
    if(token.size()<20 || token.size()>128 || token.rfind("cf_live_",0)!=0)
        throw std::invalid_argument("AUTH_INVALID");
    SqliteConnection db(path_); AuthRepository repo(db);
    const auto digest=tokenDigest(token);
    auto user=repo.userByTokenHash(digest);
    if(!user.id || user.status!="ACTIVE" || user.revoked ||
       user.tokenHash.size()!=digest.size() ||
       sodium_memcmp(user.tokenHash.data(),digest.data(),digest.size())!=0)
       throw std::invalid_argument("AUTH_INVALID");
    if(user.expired) throw std::invalid_argument("AUTH_EXPIRED");
    MeResult result; result.user=user; result.grants=repo.grants(user.id);
    if(user.role=="worker") result.workerId=repo.workerId(user.id);
    return result;
}
bool AuthService::logout(const std::string &token) {
    if(token.size()<20 || token.size()>128 || token.rfind("cf_live_",0)!=0)
        return false;
    SqliteConnection db(path_); SqliteTransaction tx(db); AuthRepository repo(db);
    const bool changed=repo.revokeSession(tokenDigest(token));
    if(changed) tx.commit();
    return changed;
}
}
