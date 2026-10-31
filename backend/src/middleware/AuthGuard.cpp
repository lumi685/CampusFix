#include "middleware/AuthGuard.h"
#include "common/ResponseUtils.h"
#include <stdexcept>

namespace campusfix {
namespace {
constexpr const char *identityKey="campusfix.auth.identity";
void reject(HttpCallback &callback,const std::string &id,drogon::HttpStatusCode status,
            const std::string &code) {
    auto response=errorResponse(status,code,"Authentication failed",id);
    response->addHeader("Cache-Control","no-store");
    callback(response);
}
}
std::string bearerToken(const drogon::HttpRequestPtr &request) {
    const auto &value=request->getHeader("Authorization");
    // Scheme is case insensitive; reject whitespace/extra tokens within credentials.
    if(value.size()<8) return {};
    std::string scheme=value.substr(0,7);
    for(auto &c:scheme) if(c>='A'&&c<='Z') c=static_cast<char>(c-'A'+'a');
    if(scheme!="bearer ") return {};
    auto token=value.substr(7);
    if(token.find_first_of(" \t\r\n")!=std::string::npos) return {};
    return token;
}
std::shared_ptr<const RequestIdentity> requestIdentity(const drogon::HttpRequestPtr &request) {
    return request->attributes()->get<std::shared_ptr<const RequestIdentity>>(identityKey);
}
GuardedHandler authenticated(const std::filesystem::path &path,GuardedHandler handler,bool databaseBusyAsUnavailable) {
    return [path,handler=std::move(handler),databaseBusyAsUnavailable](const drogon::HttpRequestPtr &request,HttpCallback &&callback) {
        const auto id=newRequestId();
        const auto token=bearerToken(request);
        if(token.empty()) {
            reject(callback,id,drogon::k401Unauthorized,
                   request->getHeader("Authorization").empty()?"AUTH_REQUIRED":"AUTH_INVALID");
            return;
        }
        if(path.empty()) {reject(callback,id,drogon::k401Unauthorized,"AUTH_INVALID");return;}
        std::shared_ptr<const RequestIdentity> identity;
        try {
            const auto me=AuthService(path).me(token);
            identity=std::make_shared<const RequestIdentity>(RequestIdentity{
                me.user.id,me.user.username,me.user.displayName,me.user.role,me.grants,me.workerId,id});
        } catch(const std::invalid_argument &error) {
            reject(callback,id,drogon::k401Unauthorized,
                   std::string(error.what())=="AUTH_EXPIRED"?"AUTH_EXPIRED":"AUTH_INVALID");return;
        } catch(const SqliteError &error) {
            if(databaseBusyAsUnavailable&&(error.primaryCode()==SQLITE_BUSY||error.primaryCode()==SQLITE_LOCKED))
                reject(callback,id,drogon::k503ServiceUnavailable,"DATABASE_BUSY");
            else reject(callback,id,drogon::k500InternalServerError,"INTERNAL_ERROR");
            return;
        } catch(const std::exception &) {
            reject(callback,id,drogon::k500InternalServerError,"INTERNAL_ERROR");return;
        }
        request->attributes()->insert(identityKey,identity);
        handler(request,std::move(callback));
    };
}
}
