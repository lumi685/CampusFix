#include "controllers/WorkOrderTimelineController.h"
#include "middleware/AuthGuard.h"
#include "common/ResponseUtils.h"
#include "services/WorkOrderService.h"
#include <charconv>

namespace campusfix {
namespace {
std::int64_t positive(const std::string &s,const char *field) {
    std::int64_t n=0;const auto parsed=std::from_chars(s.data(),s.data()+s.size(),n);
    if(parsed.ec!=std::errc{}||parsed.ptr!=s.data()+s.size()||n<=0)
        throw WorkOrderError(422,"VALIDATION_ERROR","Expected a positive integer",field);
    return n;
}
Json::Value optional(const std::optional<std::string> &v) {return v?Json::Value(*v):Json::Value(Json::nullValue);}
Json::Value data(const TimelinePage &page) {
    Json::Value result(Json::objectValue);result["page"]=Json::Int64(page.page);result["page_size"]=Json::Int64(page.pageSize);
    result["total"]=Json::Int64(page.total);result["items"]=Json::Value(Json::arrayValue);
    for(const auto &item:page.items) {
        Json::Value j(Json::objectValue);j["id"]=Json::Int64(item.id);j["action"]=item.action;
        j["from_status"]=optional(item.fromStatus);j["to_status"]=item.toStatus;j["public_note"]=optional(item.publicNote);
        j["created_at"]=item.createdAt;j["actor"]=Json::nullValue;
        if(item.actorId) {j["actor"]["id"]=Json::Int64(*item.actorId);j["actor"]["display_name"]=item.actorName;}
        if(item.internalNote) j["internal_note"]=*item.internalNote;
        if(!item.managementDetail.empty()) {
            j["management_detail"]=Json::Value(Json::objectValue);
            for(const auto &field:item.managementDetail) {
                if(const auto *n=std::get_if<std::int64_t>(&field.second)) j["management_detail"][field.first]=Json::Int64(*n);
                else j["management_detail"][field.first]=std::get<std::string>(field.second);
            }
        }
        result["items"].append(j);
    }
    return result;
}
void read(const std::filesystem::path &path,const drogon::HttpRequestPtr &request,HttpCallback &cb,const std::string &orderId) {
    const auto identity=requestIdentity(request);const auto &rid=identity->requestId;drogon::HttpResponsePtr response;
    try {
        TimelineQuery query;
        for(const auto &p:request->parameters()) {
            if(p.first=="page") query.page=positive(p.second,"page");
            else if(p.first=="page_size") query.pageSize=positive(p.second,"page_size");
            else throw WorkOrderError(422,"VALIDATION_ERROR","Unknown timeline parameter",p.first);
        }
        SqliteConnection db(path);WorkOrderService service(db);
        response=successResponse(data(service.timeline(identity->id,bearerToken(request),positive(orderId,"id"),query)),rid);
    }catch(const WorkOrderError &e) {
        response=errorResponse(static_cast<drogon::HttpStatusCode>(e.status),e.code,e.what(),rid);
        if(!e.field.empty()) {auto body=*response->getJsonObject();body["data"]["field"]=e.field;
            response=drogon::HttpResponse::newHttpJsonResponse(body);response->setStatusCode(static_cast<drogon::HttpStatusCode>(e.status));}
    }catch(const SqliteError &e) {
        const bool busy=e.primaryCode()==SQLITE_BUSY||e.primaryCode()==SQLITE_LOCKED;
        response=errorResponse(busy?drogon::k503ServiceUnavailable:drogon::k500InternalServerError,
            busy?"DATABASE_BUSY":"INTERNAL_ERROR","Timeline unavailable",rid);
    }catch(const std::exception &) {response=errorResponse(drogon::k500InternalServerError,"INTERNAL_ERROR","Timeline unavailable",rid);}
    response->addHeader("Cache-Control","no-store");LOG_INFO<<"WorkOrder timeline http="<<response->statusCode()<<" request_id="<<rid;cb(response);
}
}
void registerWorkOrderTimelineController(const std::filesystem::path &path) {
    drogon::app().registerHandler("/api/work-orders/{1}/timeline",
        [path](const drogon::HttpRequestPtr &r,HttpCallback &&c,std::string id) {
            authenticated(path,[path,id=std::move(id)](const drogon::HttpRequestPtr &req,HttpCallback &&cb){read(path,req,cb,id);})(r,std::move(c));
        },{drogon::Get});
}
}
