#include "services/WorkOrderService.h"
#include "services/AuthorizationService.h"
#include "domain/WorkOrderStateMachine.h"
#include <limits>
#include <set>

namespace campusfix {
void validateOrderQuery(const OrderQuery &q) {
    auto invalid=[](const char *field){throw WorkOrderError(422,"VALIDATION_ERROR","Invalid query parameter",field);};
    if(q.view!=OrderView::Reported&&q.view!=OrderView::Assigned&&q.view!=OrderView::Managed) invalid("view");
    if(q.currentOnly&&q.view!=OrderView::Assigned) invalid("current_only");
    if(q.page<1) invalid("page");
    if(q.pageSize<1||q.pageSize>100) invalid("page_size");
    if(q.page-1>std::numeric_limits<std::int64_t>::max()/q.pageSize) invalid("page");
    if(q.categoryId&&*q.categoryId<=0) invalid("category_id");
    if(q.status&&!WorkOrderStateMachine::knownState(*q.status)) invalid("status");
}
OrderPage WorkOrderService::list(std::int64_t user,const std::string &token,const OrderQuery &query) {
    SqliteTransaction snapshot(db_,false);
    authenticate(user,token);validateOrderQuery(query);
    const auto subject=AuthorizationRepository(db_).subject(user);
    bool allowed=false;std::string cap;
    switch(query.view) {
    case OrderView::Reported:allowed=subject.role=="reporter"||subject.role=="worker";break;
    case OrderView::Assigned:allowed=subject.role=="worker";break;
    case OrderView::Managed:
        if(subject.role=="dispatcher") cap="AREA_SCOPE";
        if(subject.role=="sysadmin") cap="BUSINESS_READ";
        if(!cap.empty()) for(const auto &grant:subject.grants) if(grant.capability==cap) allowed=true;
        break;
    }
    if(!allowed) throw WorkOrderError(403,"ACTION_FORBIDDEN","View not allowed");
    const auto result=WorkOrderRepository(db_).list(user,cap,query);
    snapshot.commit();return result;
}
OrderDetail WorkOrderService::detail(std::int64_t user,const std::string &token,std::int64_t id) {
    SqliteTransaction snapshot(db_,false);authenticate(user,token);
    if(id<=0) throw WorkOrderError(422,"VALIDATION_ERROR","Expected a positive integer","id");
    AuthorizationService authorization(db_);
    const auto decision=authorization.order(user,Action::Read,id);
    if(!decision.allowed()) throw WorkOrderError(decision.status,decision.code,"Work order not found");
    const auto fields=authorization.fields(user,id);
    const auto result=WorkOrderRepository(db_).detail(id,fields.contact);
    snapshot.commit();return result;
}
}
