#include "controllers/DispatchController.h"
#include "services/AssignmentService.h"
#include "domain/WorkOrderValidation.h"
#include "middleware/AuthGuard.h"
#include "common/ResponseUtils.h"
#include <charconv>
#include <memory>
#include <set>

namespace campusfix {
namespace {
void invalid(const char *field) {throw WorkOrderError(422,"VALIDATION_ERROR","Invalid assignment field",field);}
std::int64_t integer(const Json::Value &j,const char *key) {
    const auto &v=j[key];if((v.type()!=Json::intValue&&v.type()!=Json::uintValue)||!v.isInt64()||v.asInt64()<=0)invalid(key);return v.asInt64();
}
AssignmentInput parse(const drogon::HttpRequestPtr &r) {
    const std::string raw(r->body());validateOrderText(raw,1048576,"body",true);
    Json::CharReaderBuilder b;b["allowComments"]=false;b["collectComments"]=false;b["allowTrailingCommas"]=false;
    b["rejectDupKeys"]=true;b["failIfExtra"]=true;b["allowSpecialFloats"]=false;b["stackLimit"]=16;
    const std::unique_ptr<Json::CharReader> reader(b.newCharReader());Json::Value j;std::string errors;
    try {
        if(!reader->parse(raw.data(),raw.data()+raw.size(),&j,&errors)||!j.isObject())throw WorkOrderError(400,"MALFORMED_REQUEST","Expected a JSON object");
    }catch(const Json::Exception &) {throw WorkOrderError(400,"MALFORMED_REQUEST","Expected a JSON object");}
    const std::set<std::string> allowed={"worker_id","expected_version","reason"};
    for(const auto &key:j.getMemberNames())if(!allowed.count(key))invalid("request");
    AssignmentInput input;input.workerId=integer(j,"worker_id");input.expectedVersion=integer(j,"expected_version");
    if(j.isMember("reason")){if(!j["reason"].isString())invalid("reason");input.reason=j["reason"].asString();}
    return input;
}
void handle(const std::filesystem::path &path,const drogon::HttpRequestPtr &r,HttpCallback &cb,const std::string &rawId,bool assign) {
    const auto identity=requestIdentity(r);const auto &rid=identity->requestId;drogon::HttpResponsePtr response;
    try {
        std::int64_t id=0;const auto p=std::from_chars(rawId.data(),rawId.data()+rawId.size(),id);
        if(p.ec!=std::errc{}||p.ptr!=rawId.data()+rawId.size()||id<=0)invalid("id");
        if(!r->parameters().empty())invalid("query");
        const auto token=bearerToken(r);SqliteConnection db(path);Json::Value data;
        if(!assign) {db.execute("PRAGMA query_only=ON");data=DispatchService(db).recommend(identity->id,token,id);}
        else {
            const auto result=AssignmentService(db).assign(identity->id,token,id,parse(r),rid);
            data["order_id"]=Json::Int64(result.orderId);data["assignment_id"]=Json::Int64(result.assignmentId);
            data["worker"]["id"]=Json::Int64(result.workerId);data["worker"]["display_name"]=result.workerName;
            data["status"]=result.status;data["version"]=Json::Int64(result.version);
        }
        response=successResponse(data,rid);
    }catch(const WorkOrderError &e) {
        response=errorResponse(static_cast<drogon::HttpStatusCode>(e.status),e.code,e.what(),rid);auto body=*response->getJsonObject();
        if(!e.field.empty())body["data"]["field"]=e.field;
        if(e.currentVersion)body["data"]["current_version"]=Json::Int64(*e.currentVersion);
        if(!e.currentStatus.empty())body["data"]["current_status"]=e.currentStatus;
        const auto *detail=dynamic_cast<const DispatchError *>(&e);if(detail&&!detail->reason.empty())body["data"]["reason"]=detail->reason;
        response=drogon::HttpResponse::newHttpJsonResponse(body);response->setStatusCode(static_cast<drogon::HttpStatusCode>(e.status));
    }catch(const SqliteError &e) {
        const bool busy=e.primaryCode()==SQLITE_BUSY||e.primaryCode()==SQLITE_LOCKED;
        response=errorResponse(busy?drogon::k503ServiceUnavailable:drogon::k500InternalServerError,busy?"DATABASE_BUSY":"INTERNAL_ERROR","Dispatch operation unavailable",rid);
    }catch(const std::exception &) {response=errorResponse(drogon::k500InternalServerError,"INTERNAL_ERROR","Dispatch operation unavailable",rid);}
    response->addHeader("Cache-Control","no-store");LOG_INFO<<"Dispatch http="<<response->statusCode()<<" request_id="<<rid;cb(response);
}
}
void registerDispatchController(const std::filesystem::path &path) {
    const auto registerRoute=[&](const char *uri,bool assign,drogon::HttpMethod method) {
        drogon::app().registerHandler(uri,[path,assign](const drogon::HttpRequestPtr &r,HttpCallback &&cb,std::string id) {
            authenticated(path,[path,assign,id=std::move(id)](const drogon::HttpRequestPtr &q,HttpCallback &&c){handle(path,q,c,id,assign);},true)(r,std::move(cb));
        },{method});
    };
    registerRoute("/api/work-orders/{1}/dispatch-candidates",false,drogon::Get);
    registerRoute("/api/work-orders/{1}/assign",true,drogon::Post);
}
}
