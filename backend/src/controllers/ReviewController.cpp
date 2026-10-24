#include "controllers/ReviewController.h"
#include "common/ResponseUtils.h"
#include "middleware/AuthGuard.h"
#include "services/WorkOrderService.h"
#include <charconv>
#include <set>

namespace campusfix {
namespace {
void invalid(const std::string &field) {throw WorkOrderError(422,"VALIDATION_ERROR","Invalid request field",field);}
std::int64_t integer(const Json::Value &j,const char *key) {
    const auto &v=j[key];
    if((v.type()!=Json::intValue&&v.type()!=Json::uintValue)||!v.isInt64()||v.asInt64()<=0) invalid(key);
    return v.asInt64();
}
std::string text(const Json::Value &j,const char *key) {
    if(!j[key].isString()) invalid(key);
    return j[key].asString();
}
void handle(const std::filesystem::path &path,const drogon::HttpRequestPtr &request,HttpCallback &callback,const std::string &rawId) {
    const auto identity=requestIdentity(request);const auto &rid=identity->requestId;
    drogon::HttpResponsePtr response;
    try {
        std::int64_t id=0;
        const auto parsed=std::from_chars(rawId.data(),rawId.data()+rawId.size(),id);
        if(parsed.ec!=std::errc{}||parsed.ptr!=rawId.data()+rawId.size()||id<=0) invalid("id");
        const auto json=request->getJsonObject();
        if(!json||!json->isObject()) throw WorkOrderError(400,"MALFORMED_REQUEST","Expected a JSON object");
        const std::set<std::string> allowed={"action","expected_version","category_id","approved_urgency","reason","internal_note"};
        for(const auto &name:json->getMemberNames()) if(!allowed.count(name)) invalid(name);
        ReviewInput input;input.action=text(*json,"action");input.expectedVersion=integer(*json,"expected_version");
        if(json->isMember("category_id")) input.categoryId=integer(*json,"category_id");
        if(json->isMember("approved_urgency")) input.approvedUrgency=text(*json,"approved_urgency");
        if(json->isMember("reason")) input.reason=text(*json,"reason");
        if(json->isMember("internal_note")) input.internalNote=text(*json,"internal_note");
        SqliteConnection db(path);
        const auto result=WorkOrderService(db).review(identity->id,bearerToken(request),id,input,rid);
        Json::Value data(Json::objectValue);data["id"]=Json::Int64(result.id);data["review_action"]=result.action;
        data["status"]=result.status;data["version"]=Json::Int64(result.version);
        response=successResponse(data,rid);
    }catch(const WorkOrderError &error) {
        response=errorResponse(static_cast<drogon::HttpStatusCode>(error.status),error.code,error.what(),rid);
        if(!error.field.empty()||error.currentVersion||!error.currentStatus.empty()) {
            auto body=*response->getJsonObject();
            if(!error.field.empty()) body["data"]["field"]=error.field;
            if(error.currentVersion) body["data"]["current_version"]=Json::Int64(*error.currentVersion);
            if(!error.currentStatus.empty()) body["data"]["current_status"]=error.currentStatus;
            response=drogon::HttpResponse::newHttpJsonResponse(body);
            response->setStatusCode(static_cast<drogon::HttpStatusCode>(error.status));
        }
    }catch(const SqliteError &error) {
        const bool busy=error.primaryCode()==SQLITE_BUSY||error.primaryCode()==SQLITE_LOCKED;
        response=errorResponse(busy?drogon::k503ServiceUnavailable:drogon::k500InternalServerError,
            busy?"DATABASE_BUSY":"INTERNAL_ERROR",busy?"Database busy; retry later":"Internal server error",rid);
    }catch(const std::exception &) {
        response=errorResponse(drogon::k500InternalServerError,"INTERNAL_ERROR","Internal server error",rid);
    }
    response->addHeader("Cache-Control","no-store");
    // Never log the payload (public reasons can contain personal information too).
    LOG_INFO<<"WorkOrder review http="<<response->statusCode()<<" request_id="<<rid;
    callback(response);
}
}
void registerReviewController(const std::filesystem::path &path) {
    drogon::app().registerHandler("/api/work-orders/{1}/review",
        [path](const drogon::HttpRequestPtr &req,HttpCallback &&cb,std::string id) {
            authenticated(path,[path,id=std::move(id)](const drogon::HttpRequestPtr &r,HttpCallback &&c){handle(path,r,c,id);})(req,std::move(cb));
        },{drogon::Post});
}
}
