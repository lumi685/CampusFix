#include "services/CatalogService.h"
#include "services/AuthService.h"
#include "models/WorkOrder.h"

namespace campusfix {
std::vector<CatalogItem> CatalogService::children(const std::string &token,bool locations,std::optional<std::int64_t> parent) {
    SqliteTransaction snapshot(db_,false);
    try {AuthService::me(db_,token);}
    catch(const std::invalid_argument &e) {throw WorkOrderError(401,e.what(),"Invalid session");}
    if(parent&&*parent<=0) throw WorkOrderError(422,"VALIDATION_ERROR","Invalid parent_id","parent_id");
    auto result=CatalogRepository(db_).children(locations,parent);
    snapshot.commit();return result;
}
}
