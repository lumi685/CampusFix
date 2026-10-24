#include "services/WorkOrderService.h"
#include "services/AuthorizationService.h"
#include "domain/WorkOrderValidation.h"
#include "repositories/StatusLogRepository.h"
#include <limits>

namespace campusfix {
OrderEditResult WorkOrderService::edit(std::int64_t user,const std::string &token,std::int64_t id,
                                      std::int64_t version,const WorkOrderInput &input,const std::string &requestId) {
    if(id<=0||version<=0) throw WorkOrderError(422,"VALIDATION_ERROR","Expected a positive integer",id<=0?"id":"expected_version");
    SqliteTransaction tx(db_);authenticate(user,token);
    const auto decision=AuthorizationService(db_).operation(user,Action::Edit,id,version);
    if(!decision.allowed()) {
        WorkOrderError e(decision.status,decision.code,"Operation not permitted");
        if(decision.status==409) {
            const auto o=WorkOrderRepository(db_).find(id);
            if(decision.code=="ORDER_STATUS_CONFLICT") e.currentStatus=o.status;
            else e.currentVersion=o.version;
        }
        throw e;
    }
    WorkOrderRepository repo(db_);const auto order=repo.find(id);
    if(version==std::numeric_limits<std::int64_t>::max()) throw WorkOrderError(409,"ORDER_VERSION_CONFLICT","Order version limit reached");
    validateWorkOrderInput(input);repo.validateReferences(input);
    std::string summary;
    const auto changed=[&](bool different,const char *field){if(different){if(!summary.empty())summary+=", ";summary+=field;}};
    const auto &old=order.input;
    changed(old.categoryId!=input.categoryId,"category_id");changed(old.locationId!=input.locationId,"location_id");
    changed(old.title!=input.title,"title");changed(old.description!=input.description,"description");
    changed(old.urgency!=input.urgency,"requested_urgency");changed(old.contactName!=input.contactName,"contact_name");
    changed(old.contactPhone!=input.contactPhone,"contact_phone");changed(old.expectedVisitAt!=input.expectedVisitAt,"expected_visit_at");
    const auto utc=orderTime(clock_()).utc;repo.edit(order,input,utc);
    StatusLogEntry event;event.orderId=id;event.actorId=user;event.action="ORDER_CONTENT_UPDATED";
    event.fromStatus=order.status;event.toStatus=order.status;event.createdAt=utc;event.requestId=requestId;
    event.publicNote="Updated fields: "+(summary.empty()?"none":summary);
    StatusLogRepository(db_).append(event);tx.commit();return {id,version+1,order.status,utc};
}
}
