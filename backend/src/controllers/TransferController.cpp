#include "controllers/TransferController.h"
#include "services/TransferQueryService.h"
#include "middleware/AuthGuard.h"
#include "common/ResponseUtils.h"
#include <charconv>
#include <set>
namespace campusfix {
namespace {
std::int64_t positive(const std::string &s,const char *field) {
    std::int64_t n=0;const auto p=std::from_chars(s.data(),s.data()+s.size(),n);
    if(p.ec!=std::errc{}||p.ptr!=s.data()+s.size()||n<=0)throw WorkOrderError(422,"VALIDATION_ERROR","Expected positive int64",field);return n;
}
Json::Value optional(const std::optional<std::string> &s){return s?Json::Value(*s):Json::Value();}
Json::Value data(const TransferPage &p) {
    Json::Value j;j["page"]=Json::Int64(p.page);j["page_size"]=Json::Int64(p.pageSize);j["total"]=Json::Int64(p.total);j["items"]=Json::arrayValue;
    for(const auto &t:p.items) {
        Json::Value i;i["transfer_request_id"]=Json::Int64(t.id);i["order_id"]=Json::Int64(t.orderId);i["order_no"]=optional(t.orderNo);i["assignment_id"]=Json::Int64(t.assignmentId);
        i["reason"]=t.reason;i["status"]=t.status;i["created_at"]=t.createdAt;i["order_status"]=t.orderStatus;i["order_version"]=Json::Int64(t.orderVersion);i["review_reason"]=optional(t.reviewReason);i["resolved_at"]=optional(t.resolvedAt);j["items"].append(i);
    }
    return j;
}
void list(const std::filesystem::path &path,const drogon::HttpRequestPtr &r,HttpCallback &cb) {
    const auto identity=requestIdentity(r);const auto &rid=identity->requestId;drogon::HttpResponsePtr response;
    try {
        const auto &params=r->parameters();static const std::set<std::string> allowed={"view","page","page_size","order_id"};
        for(const auto &p:params)if(!allowed.count(p.first))throw WorkOrderError(422,"VALIDATION_ERROR","Unknown transfer query field","query");
        TransferQuery q;q.view=r->getParameter("view");if(params.count("page"))q.page=positive(r->getParameter("page"),"page");
        if(params.count("page_size"))q.pageSize=positive(r->getParameter("page_size"),"page_size");if(params.count("order_id"))q.orderId=positive(r->getParameter("order_id"),"order_id");
        SqliteConnection db(path);response=successResponse(data(TransferQueryService(db).list(identity->id,bearerToken(r),q)),rid);
    }catch(const WorkOrderError &e) {
        auto j=*errorResponse(static_cast<drogon::HttpStatusCode>(e.status),e.code,e.what(),rid)->getJsonObject();if(!e.field.empty())j["data"]["field"]=e.field;
        response=drogon::HttpResponse::newHttpJsonResponse(j);response->setStatusCode(static_cast<drogon::HttpStatusCode>(e.status));
    }catch(const SqliteError &e) {const bool busy=e.primaryCode()==SQLITE_BUSY||e.primaryCode()==SQLITE_LOCKED;response=errorResponse(busy?drogon::k503ServiceUnavailable:drogon::k500InternalServerError,busy?"DATABASE_BUSY":"INTERNAL_ERROR","Transfer query unavailable",rid);}
    catch(const std::exception &){response=errorResponse(drogon::k500InternalServerError,"INTERNAL_ERROR","Transfer query unavailable",rid);}
    response->addHeader("Cache-Control","no-store");LOG_INFO<<"Transfer query http="<<response->statusCode()<<" request_id="<<rid;cb(response);
}
}
void registerTransferController(const std::filesystem::path &path){drogon::app().registerHandler("/api/transfer-requests",authenticated(path,[path](const drogon::HttpRequestPtr &r,HttpCallback &&cb){list(path,r,cb);},true),{drogon::Get});}
}
