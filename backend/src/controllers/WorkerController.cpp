#include "controllers/WorkerController.h"
#include "services/WorkerService.h"
#include "models/WorkOrder.h"
#include "middleware/AuthGuard.h"
#include "common/ResponseUtils.h"
#include <charconv>
#include <set>

namespace campusfix {
namespace {
void invalid(const std::string &field){throw WorkOrderError(422,"VALIDATION_ERROR","Invalid worker field",field);}
std::int64_t positive(const std::string &text,const std::string &field) {
    std::int64_t n=0;const auto p=std::from_chars(text.data(),text.data()+text.size(),n);
    if(p.ec!=std::errc{}||p.ptr!=text.data()+text.size()||n<=0)invalid(field);return n;
}
void fields(const Json::Value &j,const std::set<std::string> &allowed,const std::string &prefix={}) {
    if(!j.isObject())throw WorkOrderError(400,"MALFORMED_REQUEST","Expected a JSON object");
    for(const auto &key:j.getMemberNames())if(!allowed.count(key))invalid(prefix+key);
    for(const auto &key:allowed)if(!j.isMember(key))invalid(prefix+key);
}
std::int64_t number(const Json::Value &j,const char *key,const std::string &prefix={}) {
    const auto &v=j[key];if((v.type()!=Json::intValue&&v.type()!=Json::uintValue)||!v.isInt64()||v.asInt64()<=0)invalid(prefix+key);return v.asInt64();
}
bool boolean(const Json::Value &j,const char *key,const std::string &prefix={}) {if(!j[key].isBool())invalid(prefix+key);return j[key].asBool();}
Json::Value skills(const std::vector<WorkerSkill> &items) {
    Json::Value result(Json::arrayValue);for(const auto &i:items){Json::Value row(Json::objectValue);row["skill_id"]=Json::Int64(i.skillId);row["level"]=Json::Int64(i.level);row["certified"]=i.certified;result.append(row);}return result;
}
Json::Value summary(const WorkerRecord &w) {
    Json::Value row(Json::objectValue);row["id"]=Json::Int64(w.id);row["worker_no"]=w.workerNo;row["display_name"]=w.displayName;
    row["service_area_id"]=Json::Int64(w.serviceAreaId);row["availability"]=w.availability;row["max_concurrent_tasks"]=Json::Int64(w.capacity);
    row["active_tasks"]=Json::Int64(w.activeTasks);row["skills"]=skills(w.skills);return row;
}
void handle(const std::filesystem::path &path,const drogon::HttpRequestPtr &r,HttpCallback &cb,const std::string &rawId={},bool update=false) {
    const auto identity=requestIdentity(r);const auto &rid=identity->requestId;drogon::HttpResponsePtr response;
    try {
        WorkerQuery query;WorkerInput input;std::int64_t id=0;
        if(rawId.empty()) {
            for(const auto &p:r->parameters()) {
                if(p.first=="page")query.page=positive(p.second,"page");
                else if(p.first=="page_size")query.pageSize=positive(p.second,"page_size");
                else if(p.first=="skill_id")query.skillId=positive(p.second,"skill_id");
                else if(p.first=="availability")query.availability=p.second;
                else if(p.first=="mine"){if(p.second!="true"&&p.second!="false")invalid("mine");query.mine=p.second=="true";}
                else invalid(p.first);
            }
        } else {
            id=positive(rawId,"id");if(!r->parameters().empty())invalid(r->parameters().begin()->first);
            if(update) {
                const auto json=r->getJsonObject();if(!json||!json->isObject())throw WorkOrderError(400,"MALFORMED_REQUEST","Expected a JSON object");
                fields(*json,{"service_area_id","availability","max_concurrent_tasks","emergency_duty","active","skills"});
                input.serviceAreaId=number(*json,"service_area_id");input.capacity=number(*json,"max_concurrent_tasks");
                if(!(*json)["availability"].isString())invalid("availability");input.availability=(*json)["availability"].asString();
                input.emergencyDuty=boolean(*json,"emergency_duty");input.active=boolean(*json,"active");
                if(!(*json)["skills"].isArray())invalid("skills");
                std::size_t n=0;for(const auto &s:(*json)["skills"]) {
                    const auto prefix="skills["+std::to_string(n++)+"].";
                    if(!s.isObject())invalid(prefix+"skill_id");fields(s,{"skill_id","level","certified"},prefix);
                    input.skills.push_back({number(s,"skill_id",prefix),number(s,"level",prefix),boolean(s,"certified",prefix)});
                }
            }
        }
        SqliteConnection db(path);WorkerService service(db);Json::Value data(Json::objectValue);const auto token=bearerToken(r);
        if(rawId.empty()) {
            const auto page=service.list(identity->id,token,query);data["items"]=Json::arrayValue;
            for(const auto &worker:page.items) {
                data["items"].append(summary(worker));
                if(worker.activeTasks>worker.capacity)LOG_WARN<<"Worker overload worker_id="<<worker.id<<" active_tasks="<<worker.activeTasks<<" capacity="<<worker.capacity<<" request_id="<<rid;
            }
            data["page"]=Json::Int64(page.page);data["page_size"]=Json::Int64(page.pageSize);data["total"]=Json::Int64(page.total);
        } else if(!update) {
            const auto load=service.workload(identity->id,token,id);data["worker_id"]=Json::Int64(load.workerId);data["active_tasks"]=Json::Int64(load.activeTasks);
            data["max_concurrent_tasks"]=Json::Int64(load.capacity);data["remaining_capacity"]=Json::Int64(load.remaining);data["calculated_at"]=load.calculatedAt;
            if(load.overloaded)LOG_WARN<<"Worker overload worker_id="<<id<<" active_tasks="<<load.activeTasks<<" capacity="<<load.capacity<<" request_id="<<rid;
        } else {
            const auto w=service.update(identity->id,token,id,input);data["id"]=Json::Int64(w.id);data["availability"]=w.availability;
            data["max_concurrent_tasks"]=Json::Int64(w.capacity);data["skills"]=skills(w.skills);data["updated_at"]=w.updatedAt;
        }
        response=successResponse(data,rid);
    }catch(const WorkOrderError &e) {
        response=errorResponse(static_cast<drogon::HttpStatusCode>(e.status),e.code,e.what(),rid);
        if(!e.field.empty()){auto body=*response->getJsonObject();body["data"]["field"]=e.field;response=drogon::HttpResponse::newHttpJsonResponse(body);response->setStatusCode(static_cast<drogon::HttpStatusCode>(e.status));}
    }catch(const SqliteError &e) {
        const bool busy=e.primaryCode()==SQLITE_BUSY||e.primaryCode()==SQLITE_LOCKED;
        response=errorResponse(busy?drogon::k503ServiceUnavailable:drogon::k500InternalServerError,busy?"DATABASE_BUSY":"INTERNAL_ERROR","Worker operation unavailable",rid);
    }catch(const std::exception &){response=errorResponse(drogon::k500InternalServerError,"INTERNAL_ERROR","Worker operation unavailable",rid);}
    response->addHeader("Cache-Control","no-store");LOG_INFO<<"Worker http="<<response->statusCode()<<" request_id="<<rid;cb(response);
}
}
void registerWorkerController(const std::filesystem::path &path) {
    drogon::app().registerHandler("/api/workers",authenticated(path,[path](const drogon::HttpRequestPtr &r,HttpCallback &&c){handle(path,r,c);}),{drogon::Get});
    drogon::app().registerHandler("/api/workers/{1}/workload",[path](const drogon::HttpRequestPtr &r,HttpCallback &&c,std::string id){authenticated(path,[path,id=std::move(id)](const drogon::HttpRequestPtr &q,HttpCallback &&cb){handle(path,q,cb,id);})(r,std::move(c));},{drogon::Get});
    drogon::app().registerHandler("/api/workers/{1}",[path](const drogon::HttpRequestPtr &r,HttpCallback &&c,std::string id){authenticated(path,[path,id=std::move(id)](const drogon::HttpRequestPtr &q,HttpCallback &&cb){handle(path,q,cb,id,true);})(r,std::move(c));},{drogon::Put});
}
}
