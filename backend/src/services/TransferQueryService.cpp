#include "services/TransferQueryService.h"
#include "services/WorkflowSupport.h"
#include <limits>
namespace campusfix {
TransferPage TransferQueryService::list(std::int64_t user,const std::string &token,const TransferQuery &q) {
    db_.execute("PRAGMA query_only=ON");SqliteTransaction snapshot(db_,false);workflow::authenticate(db_,user,token);
    const auto invalid=[](const char *field){throw WorkOrderError(422,"VALIDATION_ERROR","Invalid transfer query",field);};
    if(q.view!="mine"&&q.view!="managed")invalid("view");
    if(q.page<1)invalid("page");if(q.pageSize<1||q.pageSize>100)invalid("page_size");
    if(q.page-1>std::numeric_limits<std::int64_t>::max()/q.pageSize)invalid("page");if(q.orderId&&*q.orderId<=0)invalid("order_id");
    const auto s=AuthorizationRepository(db_).subject(user);std::string cap;
    if(q.view=="mine") {if(s.role!="worker")throw WorkOrderError(403,"ACTION_FORBIDDEN","View unavailable");}
    else {
        if(s.role=="dispatcher")cap="AREA_SCOPE";else if(s.role=="sysadmin")cap="DISPATCH";
        bool granted=false;for(const auto &g:s.grants)if(!cap.empty()&&g.capability==cap)granted=true;
        if(!granted)throw WorkOrderError(403,"ACTION_FORBIDDEN","View unavailable");
    }
    const auto result=TransferRepository(db_).list(user,cap,q);snapshot.commit();return result;
}
}
