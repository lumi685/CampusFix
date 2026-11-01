#include "controllers/WorkflowController.h"
#include "services/TransferService.h"
#include "services/ProgressEventService.h"
#include "services/CompletionService.h"
#include "domain/WorkOrderValidation.h"
#include "middleware/AuthGuard.h"
#include "common/ResponseUtils.h"
#include <charconv>
#include <memory>
#include <set>
namespace campusfix {
namespace {
enum class Kind {Create,Decide,Progress,Complete};
void invalid(const char *field){throw WorkOrderError(422,"VALIDATION_ERROR","Invalid workflow field",field);}
std::int64_t positive(const std::string &s,const char *field){std::int64_t n=0;const auto p=std::from_chars(s.data(),s.data()+s.size(),n);if(p.ec!=std::errc{}||p.ptr!=s.data()+s.size()||n<=0)invalid(field);return n;}
void perform(const std::filesystem::path &path,const drogon::HttpRequestPtr &r,HttpCallback &cb,const std::string &rawId,const std::string &rawRequest,Kind kind){
    const auto identity=requestIdentity(r);const auto &rid=identity->requestId;drogon::HttpResponsePtr response;
    try {
        const auto id=positive(rawId,"id");const auto request=kind==Kind::Decide?positive(rawRequest,"request_id"):0;if(!r->parameters().empty())invalid("query");
        const std::string body(r->body());validateOrderText(body,1048576,"body",true);Json::CharReaderBuilder b;b["allowComments"]=false;b["allowTrailingCommas"]=false;b["collectComments"]=false;b["rejectDupKeys"]=true;b["failIfExtra"]=true;b["allowSpecialFloats"]=false;b["stackLimit"]=16;
        const std::unique_ptr<Json::CharReader> parser(b.newCharReader());Json::Value j;std::string errors;
        try{if(!parser->parse(body.data(),body.data()+body.size(),&j,&errors)||!j.isObject())throw WorkOrderError(400,"MALFORMED_REQUEST","Expected JSON object");}catch(const Json::Exception &){throw WorkOrderError(400,"MALFORMED_REQUEST","Expected JSON object");}
        const std::set<std::string> allowed=kind==Kind::Complete?std::set<std::string>{"expected_version","repair_summary"}:kind==Kind::Progress?std::set<std::string>{"expected_version","type","note"}:kind==Kind::Decide?std::set<std::string>{"expected_version","decision","reason"}:std::set<std::string>{"expected_version","reason"};
        for(const auto &key:j.getMemberNames())if(!allowed.count(key))invalid("request");const auto &v=j["expected_version"];if((v.type()!=Json::intValue&&v.type()!=Json::uintValue)||!v.isInt64()||v.asInt64()<=0)invalid("expected_version");
        SqliteConnection db(path);Json::Value data;const auto token=bearerToken(r);
        if(kind==Kind::Complete){if(!j["repair_summary"].isString())invalid("repair_summary");const auto result=CompletionService(db).complete(identity->id,token,id,v.asInt64(),j["repair_summary"].asString(),rid);data["order_id"]=Json::Int64(result.orderId);data["status"]=result.status;data["version"]=Json::Int64(result.version);data["repair_summary"]=result.summary;
        }else if(kind==Kind::Progress){if(!j["type"].isString())invalid("type");std::optional<std::string> note;if(j.isMember("note")){if(!j["note"].isString())invalid("note");note=j["note"].asString();}
            const auto e=ProgressEventService(db).append(identity->id,token,id,v.asInt64(),j["type"].asString(),note,rid);data["id"]=Json::Int64(e.id);data["order_id"]=Json::Int64(e.orderId);data["assignment_id"]=Json::Int64(e.assignmentId);data["type"]=e.type;data["order_status"]=e.orderStatus;data["order_version"]=Json::Int64(e.orderVersion);data["created_at"]=e.createdAt;
        }else{if(!j["reason"].isString())invalid("reason");if(kind==Kind::Create){const auto t=TransferService(db).create(identity->id,token,id,v.asInt64(),j["reason"].asString(),rid);data["transfer_request_id"]=Json::Int64(t.id);data["order_id"]=Json::Int64(t.orderId);data["status"]="PENDING";data["order_status"]=t.orderStatus;}
            else{if(!j["decision"].isString())invalid("decision");const auto t=TransferService(db).decide(identity->id,token,id,request,v.asInt64(),j["decision"].asString(),j["reason"].asString(),rid);data["transfer_request_id"]=Json::Int64(t.id);data["decision"]=t.decision;data["order_status"]=t.orderStatus;data["version"]=Json::Int64(t.version);data["assignment_ended"]=t.assignmentEnded;}}
        response=successResponse(data,rid);response->setStatusCode(kind==Kind::Decide||kind==Kind::Complete?drogon::k200OK:drogon::k201Created);
    }catch(const WorkOrderError &e){auto j=*errorResponse(static_cast<drogon::HttpStatusCode>(e.status),e.code,e.what(),rid)->getJsonObject();if(!e.field.empty())j["data"]["field"]=e.field;if(e.currentVersion)j["data"]["current_version"]=Json::Int64(*e.currentVersion);if(!e.currentStatus.empty())j["data"]["current_status"]=e.currentStatus;if(const auto *duplicate=dynamic_cast<const TransferDuplicate *>(&e))j["data"]["transfer_request_id"]=Json::Int64(duplicate->requestId);response=drogon::HttpResponse::newHttpJsonResponse(j);response->setStatusCode(static_cast<drogon::HttpStatusCode>(e.status));}
    catch(const SqliteError &e){const bool busy=e.primaryCode()==SQLITE_BUSY||e.primaryCode()==SQLITE_LOCKED;response=errorResponse(busy?drogon::k503ServiceUnavailable:drogon::k500InternalServerError,busy?"DATABASE_BUSY":"INTERNAL_ERROR","Workflow operation unavailable",rid);}
    catch(const std::exception &){response=errorResponse(drogon::k500InternalServerError,"INTERNAL_ERROR","Workflow operation unavailable",rid);}
    response->addHeader("Cache-Control","no-store");LOG_INFO<<"Workflow http="<<response->statusCode()<<" request_id="<<rid;cb(response);
}
}
void registerWorkflowController(const std::filesystem::path &path){
    for(const auto kind:{Kind::Create,Kind::Progress,Kind::Complete})drogon::app().registerHandler(kind==Kind::Create?"/api/work-orders/{1}/transfer-requests":kind==Kind::Complete?"/api/work-orders/{1}/complete":"/api/work-orders/{1}/progress-events",[path,kind](const drogon::HttpRequestPtr &r,HttpCallback &&cb,std::string id){authenticated(path,[path,kind,id=std::move(id)](const drogon::HttpRequestPtr &q,HttpCallback &&c){perform(path,q,c,id,{},kind);},true)(r,std::move(cb));},{drogon::Post});
    drogon::app().registerHandler("/api/work-orders/{1}/transfer-requests/{2}/decision",[path](const drogon::HttpRequestPtr &r,HttpCallback &&cb,std::string id,std::string request){authenticated(path,[path,id=std::move(id),request=std::move(request)](const drogon::HttpRequestPtr &q,HttpCallback &&c){perform(path,q,c,id,request,Kind::Decide);},true)(r,std::move(cb));},{drogon::Post});
}
}
