#include "controllers/WorkOrderController.h"
#include "controllers/CatalogController.h"
#include "controllers/WorkOrderQueryController.h"
#include "common/ResponseUtils.h"
#include "middleware/AuthGuard.h"
#include "services/WorkOrderService.h"
#include <charconv>
#include <memory>
#include <set>

namespace campusfix {
namespace {
void invalid(const std::string &field) {throw WorkOrderError(422,"VALIDATION_ERROR","Invalid request field",field);}
void fields(const Json::Value &json,const std::set<std::string> &allowed) {
    if(!json.isObject()) throw WorkOrderError(400,"MALFORMED_REQUEST","Expected a JSON object");
    for(const auto &name:json.getMemberNames()) if(!allowed.count(name)) invalid(name);
}
std::int64_t integer(const Json::Value &j,const char *key) {
    const auto &value=j[key];
    if((value.type()!=Json::intValue&&value.type()!=Json::uintValue)||!value.isInt64()||value.asInt64()<=0) invalid(key);
    return value.asInt64();
}
std::string text(const Json::Value &j,const char *key) {
    if(!j[key].isString()) invalid(key);
    return j[key].asString();
}
Json::Value resultData(const WorkOrder &o,bool submitted) {
    Json::Value data(Json::objectValue);data["id"]=Json::Int64(o.id);
    data["order_no"]=o.orderNo?Json::Value(*o.orderNo):Json::Value(Json::nullValue);
    data["status"]=o.status;data["version"]=Json::Int64(o.version);
    if(submitted) data["submitted_at"]=o.submittedAt?Json::Value(*o.submittedAt):Json::Value(Json::nullValue);
    return data;
}
void handle(const std::filesystem::path &path,const drogon::HttpRequestPtr &request,
            HttpCallback &callback,const std::string &orderId={}) {
    const auto identity=requestIdentity(request);const auto &rid=identity->requestId;
    std::unique_ptr<SqliteConnection> db;
    drogon::HttpResponsePtr response;
    try {
        const auto json=request->getJsonObject();
        if(!json) throw WorkOrderError(400,"MALFORMED_REQUEST","Invalid JSON");
        WorkOrderInput input;std::int64_t id=0,version=0;
        if(orderId.empty()) {
            fields(*json,{"category_id","location_id","title","description","requested_urgency",
                "contact_name","contact_phone","expected_visit_at"});
            input={integer(*json,"category_id"),integer(*json,"location_id"),text(*json,"title"),
                text(*json,"description"),text(*json,"requested_urgency"),text(*json,"contact_name"),text(*json,"contact_phone"),{}};
            if(json->isMember("expected_visit_at")&&!(*json)["expected_visit_at"].isNull())
                input.expectedVisitAt=text(*json,"expected_visit_at");
        } else {
            fields(*json,{"expected_version"});version=integer(*json,"expected_version");
            const auto parsed=std::from_chars(orderId.data(),orderId.data()+orderId.size(),id);
            if(parsed.ec!=std::errc{}||parsed.ptr!=orderId.data()+orderId.size()||id<=0) invalid("id");
        }
        db=std::make_unique<SqliteConnection>(path);
        WorkOrderService service(*db);
        const auto token=bearerToken(request);
        const auto order=orderId.empty()?service.create(identity->id,token,input,rid):service.submit(identity->id,token,id,version,rid);
        response=successResponse(resultData(order,!orderId.empty()),rid);
        if(orderId.empty()) response->setStatusCode(drogon::k201Created);
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
        if(error.status>=500) LOG_ERROR<<"WorkOrder failed code="<<error.code<<" request_id="<<rid;
    }catch(const SqliteError &error) {
        const bool busy=error.primaryCode()==SQLITE_BUSY||error.primaryCode()==SQLITE_LOCKED;
        response=errorResponse(busy?drogon::k503ServiceUnavailable:drogon::k500InternalServerError,
            busy?"DATABASE_BUSY":"INTERNAL_ERROR",busy?"Database busy; retry later":"Internal server error",rid);
    }catch(const std::exception &) {
        response=errorResponse(drogon::k500InternalServerError,"INTERNAL_ERROR","Internal server error",rid);
    }
    if(response->statusCode()>=500) LOG_ERROR<<"WorkOrder infrastructure failure http="<<response->statusCode()<<" request_id="<<rid;
    response->addHeader("Cache-Control","no-store");
    LOG_INFO<<"WorkOrder "<<(orderId.empty()?"create":"submit")<<" http="<<response->statusCode()<<" request_id="<<rid;
    callback(response);
}
}
void registerWorkOrderController(const std::filesystem::path &path) {
    registerCatalogController(path);
    registerWorkOrderQueryController(path);
    drogon::app().registerHandler("/api/work-orders",authenticated(path,
        [path](const drogon::HttpRequestPtr &req,HttpCallback &&cb){handle(path,req,cb);}),{drogon::Post});
    // Drogon path extraction wraps the existing reusable Guard, never a second auth implementation.
    drogon::app().registerHandler("/api/work-orders/{1}/submit",
        [path](const drogon::HttpRequestPtr &req,HttpCallback &&cb,std::string id) {
            authenticated(path,[path,id=std::move(id)](const drogon::HttpRequestPtr &r,HttpCallback &&c){handle(path,r,c,id);})(req,std::move(cb));
        },{drogon::Post});
}
}
