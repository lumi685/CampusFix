#include "controllers/WorkerTaskController.h"
#include "services/WorkerTaskService.h"
#include "models/WorkOrder.h"
#include "domain/WorkOrderValidation.h"
#include "middleware/AuthGuard.h"
#include "common/ResponseUtils.h"
#include <charconv>
#include <memory>
namespace campusfix {
namespace {
void invalid(const char *field){throw WorkOrderError(422,"VALIDATION_ERROR","Invalid task request field",field);}
void perform(const std::filesystem::path &path,const drogon::HttpRequestPtr &r,HttpCallback &cb,const std::string &rawId,bool refuse) {
    const auto identity=requestIdentity(r);const auto &rid=identity->requestId;drogon::HttpResponsePtr response;
    try {
        std::int64_t id=0;const auto p=std::from_chars(rawId.data(),rawId.data()+rawId.size(),id);if(p.ec!=std::errc{}||p.ptr!=rawId.data()+rawId.size()||id<=0)invalid("id");
        if(!r->parameters().empty())invalid("query");const std::string body(r->body());validateOrderText(body,1048576,"body",true);
        Json::CharReaderBuilder b;b["allowComments"]=false;b["allowTrailingCommas"]=false;b["collectComments"]=false;b["rejectDupKeys"]=true;b["failIfExtra"]=true;b["allowSpecialFloats"]=false;b["stackLimit"]=16;
        const std::unique_ptr<Json::CharReader> parser(b.newCharReader());Json::Value j;std::string errors;
        try{if(!parser->parse(body.data(),body.data()+body.size(),&j,&errors)||!j.isObject())throw WorkOrderError(400,"MALFORMED_REQUEST","Expected a JSON object");}
        catch(const Json::Exception &){throw WorkOrderError(400,"MALFORMED_REQUEST","Expected a JSON object");}
        for(const auto &key:j.getMemberNames())if(key!="expected_version"&&!(refuse&&key=="reason"))invalid("request");
        const auto &v=j["expected_version"];if((v.type()!=Json::intValue&&v.type()!=Json::uintValue)||!v.isInt64()||v.asInt64()<=0)invalid("expected_version");
        std::optional<std::string> reason;if(refuse){if(!j["reason"].isString())invalid("reason");reason=j["reason"].asString();}
        SqliteConnection db(path);const auto result=WorkerTaskService(db).perform(identity->id,bearerToken(r),id,v.asInt64(),reason,rid);
        Json::Value data;data["order_id"]=Json::Int64(result.orderId);data["status"]=result.status;data["version"]=Json::Int64(result.version);
        if(refuse)data["assignment_ended"]=result.assignmentEnded;else data["accepted_at"]=result.acceptedAt;response=successResponse(data,rid);
    }catch(const WorkOrderError &e){response=errorResponse(static_cast<drogon::HttpStatusCode>(e.status),e.code,e.what(),rid);auto body=*response->getJsonObject();if(!e.field.empty())body["data"]["field"]=e.field;if(e.currentVersion)body["data"]["current_version"]=Json::Int64(*e.currentVersion);if(!e.currentStatus.empty())body["data"]["current_status"]=e.currentStatus;response=drogon::HttpResponse::newHttpJsonResponse(body);response->setStatusCode(static_cast<drogon::HttpStatusCode>(e.status));}
    catch(const SqliteError &e){const bool busy=e.primaryCode()==SQLITE_BUSY||e.primaryCode()==SQLITE_LOCKED;response=errorResponse(busy?drogon::k503ServiceUnavailable:drogon::k500InternalServerError,busy?"DATABASE_BUSY":"INTERNAL_ERROR","Task operation unavailable",rid);}
    catch(const std::exception &){response=errorResponse(drogon::k500InternalServerError,"INTERNAL_ERROR","Task operation unavailable",rid);}
    response->addHeader("Cache-Control","no-store");LOG_INFO<<"Worker task http="<<response->statusCode()<<" request_id="<<rid;cb(response);
}
}
void registerWorkerTaskController(const std::filesystem::path &path){
    for(const bool refuse:{false,true})drogon::app().registerHandler(refuse?"/api/work-orders/{1}/refuse":"/api/work-orders/{1}/accept",[path,refuse](const drogon::HttpRequestPtr &r,HttpCallback &&cb,std::string id){authenticated(path,[path,refuse,id=std::move(id)](const drogon::HttpRequestPtr &q,HttpCallback &&c){perform(path,q,c,id,refuse);},true)(r,std::move(cb));},{drogon::Post});
}
}
