#include "controllers/CatalogController.h"
#include "services/CatalogService.h"
#include "models/WorkOrder.h"
#include "middleware/AuthGuard.h"
#include "common/ResponseUtils.h"
#include <charconv>

namespace campusfix {
void registerCatalogController(const std::filesystem::path &path) {
    for(const bool locations:{true,false}) {
        drogon::app().registerHandler(locations?"/api/locations":"/api/fault-categories",authenticated(path,
            [path,locations](const drogon::HttpRequestPtr &r,HttpCallback &&cb) {
                const auto rid=requestIdentity(r)->requestId;drogon::HttpResponsePtr response;
                try {
                    std::optional<std::int64_t> parent;
                    for(const auto &p:r->parameters()) {
                        std::int64_t value=0;
                        const auto parsed=std::from_chars(p.second.data(),p.second.data()+p.second.size(),value);
                        if(p.first!="parent_id"||parsed.ec!=std::errc{}||parsed.ptr!=p.second.data()+p.second.size()||value<=0)
                            throw WorkOrderError(422,"VALIDATION_ERROR","Invalid catalog parameter",p.first);
                        parent=value;
                    }
                    SqliteConnection db(path);CatalogService service(db);Json::Value data(Json::objectValue);
                    data["items"]=Json::Value(Json::arrayValue);
                    for(const auto &item:service.children(bearerToken(r),locations,parent)) {
                        Json::Value j(Json::objectValue);j["id"]=Json::Int64(item.id);j["code"]=item.code;j["name"]=item.name;
                        if(locations) {j["location_type"]=item.kind;j["parent_id"]=item.parentId?Json::Value(Json::Int64(*item.parentId)):Json::Value(Json::nullValue);}
                        else j["default_priority"]=item.kind;
                        data["items"].append(j);
                    }
                    response=successResponse(data,rid);
                }catch(const WorkOrderError &e) {
                    response=errorResponse(static_cast<drogon::HttpStatusCode>(e.status),e.code,e.what(),rid);
                    if(!e.field.empty()) {
                        auto body=*response->getJsonObject();body["data"]["field"]=e.field;
                        response=drogon::HttpResponse::newHttpJsonResponse(body);
                        response->setStatusCode(static_cast<drogon::HttpStatusCode>(e.status));
                    }
                }
                catch(const SqliteError &e) {
                    const bool busy=e.primaryCode()==SQLITE_BUSY||e.primaryCode()==SQLITE_LOCKED;
                    response=errorResponse(busy?drogon::k503ServiceUnavailable:drogon::k500InternalServerError,busy?"DATABASE_BUSY":"INTERNAL_ERROR","Catalog unavailable",rid);
                }catch(const std::exception &) {response=errorResponse(drogon::k500InternalServerError,"INTERNAL_ERROR","Catalog unavailable",rid);}
                response->addHeader("Cache-Control","no-store");
                LOG_INFO<<"Catalog http="<<response->statusCode()<<" request_id="<<rid;cb(response);
            }),{drogon::Get});
    }
}
}
