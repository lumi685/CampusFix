#include "controllers/AuthController.h"
#include "common/ResponseUtils.h"
#include "services/AuthService.h"
#include <drogon/drogon.h>
#include <functional>
#include <memory>
#include <stdexcept>
#include <string>

namespace campusfix {
namespace {
using Callback=std::function<void(const drogon::HttpResponsePtr &)>;
void send(const drogon::HttpResponsePtr &response,Callback &callback) {
    response->addHeader("Cache-Control","no-store");
    response->addHeader("Pragma","no-cache"); callback(response);
}
std::string bearer(const drogon::HttpRequestPtr &request) {
    const std::string value=request->getHeader("Authorization");
    if(value.rfind("Bearer ",0)!=0 || value.size()<=7) return {};
    return value.substr(7);
}
Json::Value identity(const AuthUser &user) {
    Json::Value data(Json::objectValue);
    data["id"]=Json::Int64(user.id); data["username"]=user.username;
    data["display_name"]=user.displayName; data["role"]=user.role;
    return data;
}
void failure(Callback &cb,const std::string &requestId,drogon::HttpStatusCode status,
             const std::string &code,const std::string &message) {
    send(errorResponse(status,code,message,requestId),cb);
}
}
void registerAuthController(const std::filesystem::path &databasePath) {
    auto service=std::make_shared<AuthService>(databasePath);
    drogon::app().registerHandler("/api/auth/login",
      [service](const drogon::HttpRequestPtr &request,Callback &&cb) {
        const auto id=newRequestId();
        try {
            const auto json=request->getJsonObject();
            if(!json) {failure(cb,id,drogon::k400BadRequest,"MALFORMED_REQUEST","Invalid JSON");return;}
            if(!(*json)["username"].isString() || !(*json)["password"].isString()) {
                failure(cb,id,drogon::k422UnprocessableEntity,"VALIDATION_ERROR","Invalid credentials format");return;
            }
            const auto name=(*json)["username"].asString();
            const auto password=(*json)["password"].asString();
            if(name.size()<3 || name.size()>64 || password.empty() || password.size()>128 ||
               name.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_.-")!=std::string::npos) {
                failure(cb,id,drogon::k422UnprocessableEntity,"VALIDATION_ERROR","Invalid credentials format");return;
            }
            auto result=service->login(name,password);
            Json::Value data(Json::objectValue); data["token"]=result.token;
            data["expires_at"]=result.expiresAt;
            data["user"]=identity(result.user); data["user"].removeMember("username");
            send(successResponse(data,id),cb);
        } catch(const std::invalid_argument &) {
            failure(cb,id,drogon::k401Unauthorized,"AUTH_INVALID","用户名或密码错误");
        } catch(const std::exception &) {
            failure(cb,id,drogon::k500InternalServerError,"INTERNAL_ERROR","Internal server error");
        }
      },{drogon::Post});
    drogon::app().registerHandler("/api/auth/me",
      [service](const drogon::HttpRequestPtr &request,Callback &&cb) {
        const auto id=newRequestId(); const auto token=bearer(request);
        if(token.empty()) {failure(cb,id,drogon::k401Unauthorized,"AUTH_REQUIRED","请先登录");return;}
        try {
            auto result=service->me(token);
            Json::Value data=identity(result.user);
            data["capabilities"]=Json::Value(Json::arrayValue);
            data["scope_location_ids"]=Json::Value(Json::arrayValue);
            for(const auto &grant:result.grants) {
                data["capabilities"].append(grant.capability);
                if(!grant.global) data["scope_location_ids"].append(Json::Int64(grant.locationId));
            }
            if(result.workerId) data["worker_id"]=Json::Int64(result.workerId);
            send(successResponse(data,id),cb);
        } catch(const std::invalid_argument &error) {
            const bool expired=std::string(error.what())=="AUTH_EXPIRED";
            failure(cb,id,drogon::k401Unauthorized,expired?"AUTH_EXPIRED":"AUTH_INVALID",
                    expired?"登录已过期":"Invalid session");
        } catch(const std::exception &) {
            failure(cb,id,drogon::k500InternalServerError,"INTERNAL_ERROR","Internal server error");
        }
      },{drogon::Get});
    drogon::app().registerHandler("/api/auth/logout",
      [service](const drogon::HttpRequestPtr &request,Callback &&cb) {
        const auto id=newRequestId(); const auto token=bearer(request);
        if(token.empty()) {failure(cb,id,drogon::k401Unauthorized,"AUTH_REQUIRED","请先登录");return;}
        try {
            if(!service->logout(token)) {
                failure(cb,id,drogon::k401Unauthorized,"AUTH_INVALID","Invalid session");return;
            }
            Json::Value data(Json::objectValue); data["revoked"]=true;
            send(successResponse(data,id),cb);
        } catch(const std::exception &) {
            failure(cb,id,drogon::k500InternalServerError,"INTERNAL_ERROR","Internal server error");
        }
      },{drogon::Post});
}
}
