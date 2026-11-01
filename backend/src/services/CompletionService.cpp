#include "services/CompletionService.h"
#include "services/WorkflowSupport.h"
#include "repositories/CompletionRepository.h"
#include "repositories/WorkOrderRepository.h"
#include "repositories/StatusLogRepository.h"
#include "domain/WorkOrderStateMachine.h"
#include <limits>
namespace campusfix {
CompletionResult CompletionService::complete(std::int64_t user,const std::string &token,std::int64_t id,std::int64_t version,const std::string &summary,const std::string &rid) {
    if(id<=0||version<=0)throw WorkOrderError(422,"VALIDATION_ERROR","Expected positive int64");
    SqliteTransaction tx(db_);workflow::authenticate(db_,user,token);AuthorizationService auth(db_);
    workflow::require(auth.order(user,Action::Complete,id));const auto order=WorkOrderRepository(db_).find(id);
    workflow::require(auth.operation(user,Action::Complete,id,version));workflow::note(summary,2000,"repair_summary");
    const auto *rule=WorkOrderStateMachine::find(order.status,WorkflowAction::Complete);
    if(!rule)throw WorkOrderError(409,"ORDER_STATUS_CONFLICT","Completion unavailable");
    TransferRepository assignments(db_);const auto a=assignments.current(id);
    if(a.userId!=user)throw WorkOrderError(403,"ACTION_FORBIDDEN","Assignment changed");
    if(!a.acceptedAt||!validUtcDateTime(*a.acceptedAt))throw WorkOrderError(409,"ORDER_STATUS_CONFLICT","Task not accepted");
    if(order.version==std::numeric_limits<std::int64_t>::max()||a.version==std::numeric_limits<std::int64_t>::max())throw WorkOrderError(409,"ORDER_VERSION_CONFLICT","Version limit reached");
    const auto utc=orderTime(clock_()).utc;CompletionRepository repo(db_);
    repo.save(id,version,summary,utc);repo.submit(a,id,user,utc);repo.supersede(id,a.id,utc);
    StatusLogEntry e;e.orderId=id;e.actorId=user;e.assignmentId=a.id;e.action="ORDER_COMPLETED";
    e.fromStatus=order.status;e.toStatus=std::string(rule->to);e.publicNote=summary;e.createdAt=utc;e.requestId=rid;StatusLogRepository(db_).append(e);
    if(sqlite3_changes(db_.nativeHandle())!=1)throw WorkOrderError(409,"ORDER_STATUS_CONFLICT","Completion audit not appended");
    // Recheck permission, not operation state: our own T-19 has changed state.
    workflow::authenticate(db_,user,token);workflow::require(auth.order(user,Action::Complete,id));
    const auto current=assignments.current(id);
    if(current.id!=a.id||current.userId!=user||current.version!=a.version+1||current.acceptedAt!=a.acceptedAt||!repo.saved(id,version+1,a,summary,utc))
        throw WorkOrderError(409,"ORDER_STATUS_CONFLICT","Assignment changed");
    tx.commit();return {id,version+1,std::string(rule->to),summary};
}
}
