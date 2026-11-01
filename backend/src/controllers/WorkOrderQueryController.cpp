#include "controllers/WorkOrderQueryController.h"
#include "controllers/WorkOrderTimelineController.h"
#include "middleware/AuthGuard.h"
#include "common/ResponseUtils.h"
#include "services/WorkOrderService.h"
#include <charconv>
#include <set>

namespace campusfix {
namespace {
std::int64_t positive(const std::string &s,const char *field) {
    std::int64_t result=0;const auto parsed=std::from_chars(s.data(),s.data()+s.size(),result);
    if(parsed.ec!=std::errc{}||parsed.ptr!=s.data()+s.size()||result<=0)
        throw WorkOrderError(422,"VALIDATION_ERROR","Expected a positive integer",field);
    return result;
}
Json::Value optional(const std::optional<std::string> &s) {return s?Json::Value(*s):Json::Value(Json::nullValue);}
Json::Value reference(const OrderReference &r) {
    Json::Value j(Json::objectValue);j["id"]=Json::Int64(r.id);j["code"]=r.code;j["name"]=r.name;return j;
}
Json::Value summary(const OrderSummary &s) {
    Json::Value j(Json::objectValue);j["id"]=Json::Int64(s.id);j["order_no"]=optional(s.orderNo);
    j["title"]=s.title;j["category"]=reference(s.category);j["location"]=reference(s.location);
    j["requested_urgency"]=s.urgency;j["approved_urgency"]=optional(s.approvedUrgency);
    j["status"]=s.status;j["version"]=Json::Int64(s.version);j["created_at"]=s.createdAt;
    j["submitted_at"]=optional(s.submittedAt);j["updated_at"]=s.updatedAt;return j;
}
OrderQuery query(const drogon::HttpRequestPtr &request) {
    const auto &params=request->parameters();
    static const std::set<std::string> names={"view","page","page_size","status","category_id","current_only"};
    for(const auto &p:params) if(!names.count(p.first)) throw WorkOrderError(422,"VALIDATION_ERROR","Unknown query parameter",p.first);
    OrderQuery q;const auto view=request->getParameter("view");
    if(view=="reported") q.view=OrderView::Reported;
    else if(view=="assigned") q.view=OrderView::Assigned;
    else if(view=="managed") q.view=OrderView::Managed;
    else throw WorkOrderError(422,"VALIDATION_ERROR","Specify reported, assigned or managed view","view");
    if(params.count("current_only")) {
        const auto flag=request->getParameter("current_only");
        if(q.view!=OrderView::Assigned||(flag!="true"&&flag!="false"))throw WorkOrderError(422,"VALIDATION_ERROR","current_only is an assigned-view boolean","current_only");
        q.currentOnly=flag=="true";
    }
    if(params.count("page")) q.page=positive(request->getParameter("page"),"page");
    if(params.count("page_size")) q.pageSize=positive(request->getParameter("page_size"),"page_size");
    if(params.count("category_id")) q.categoryId=positive(request->getParameter("category_id"),"category_id");
    if(params.count("status")) q.status=request->getParameter("status");
    validateOrderQuery(q);return q;
}
void read(const std::filesystem::path &path,const drogon::HttpRequestPtr &request,HttpCallback &callback,
          const std::optional<std::string> &orderId={}) {
    const auto identity=requestIdentity(request);const auto &rid=identity->requestId;
    drogon::HttpResponsePtr response;
    try {
        SqliteConnection db(path);WorkOrderService service(db);Json::Value data(Json::objectValue);
        const auto token=bearerToken(request);
        if(!orderId) {
            const auto page=service.list(identity->id,token,query(request));
            data["items"]=Json::Value(Json::arrayValue);
            for(const auto &s:page.items) data["items"].append(summary(s));
            data["page"]=Json::Int64(page.page);data["page_size"]=Json::Int64(page.pageSize);data["total"]=Json::Int64(page.total);
        } else {
            const auto detail=service.detail(identity->id,token,positive(*orderId,"id"));data=summary(detail.summary);
            data["attachments"]=Json::Value(Json::arrayValue);
            if(detail.contact) {
                data["description"]=detail.contact->description;data["contact_name"]=detail.contact->name;
                data["contact_phone"]=detail.contact->phone;data["expected_visit_at"]=optional(detail.contact->expectedVisitAt);
                data["current_assignment"]=Json::nullValue;
                if(detail.currentAssignment) {
                    data["current_assignment"]["worker_id"]=Json::Int64(detail.currentAssignment->workerId);
                    data["current_assignment"]["worker_name"]=detail.currentAssignment->workerName;
                }
            }
        }
        response=successResponse(data,rid);
    }catch(const WorkOrderError &e) {
        response=errorResponse(static_cast<drogon::HttpStatusCode>(e.status),e.code,e.what(),rid);
        if(!e.field.empty()) {
            auto body=*response->getJsonObject();body["data"]["field"]=e.field;
            response=drogon::HttpResponse::newHttpJsonResponse(body);response->setStatusCode(static_cast<drogon::HttpStatusCode>(e.status));
        }
    }catch(const SqliteError &e) {
        const bool busy=e.primaryCode()==SQLITE_BUSY||e.primaryCode()==SQLITE_LOCKED;
        response=errorResponse(busy?drogon::k503ServiceUnavailable:drogon::k500InternalServerError,
            busy?"DATABASE_BUSY":"INTERNAL_ERROR","Query unavailable",rid);
    }catch(const std::exception &) {response=errorResponse(drogon::k500InternalServerError,"INTERNAL_ERROR","Query unavailable",rid);}
    response->addHeader("Cache-Control","no-store");
    LOG_INFO<<"WorkOrder query http="<<response->statusCode()<<" request_id="<<rid;
    callback(response);
}
}
void registerWorkOrderQueryController(const std::filesystem::path &path) {
    registerWorkOrderTimelineController(path);
    drogon::app().registerHandler("/api/work-orders",authenticated(path,
        [path](const drogon::HttpRequestPtr &r,HttpCallback &&c){read(path,r,c);}),{drogon::Get});
    drogon::app().registerHandler("/api/work-orders/{1}",
        [path](const drogon::HttpRequestPtr &r,HttpCallback &&c,std::string id) {
            authenticated(path,[path,id=std::move(id)](const drogon::HttpRequestPtr &q,HttpCallback &&cb){read(path,q,cb,id);})(r,std::move(c));
        },{drogon::Get});
}
}
