#include "services/ProgressEventService.h"
#include "services/WorkflowSupport.h"
#include "repositories/StatusLogRepository.h"
#include "repositories/WorkOrderRepository.h"
#include "domain/WorkOrderStateMachine.h"
namespace campusfix {
ProgressEvent ProgressEventService::append(std::int64_t user,const std::string &token,std::int64_t id,std::int64_t version,const std::string &type,
                                          const std::optional<std::string> &note,const std::string &rid) {
    if(id<=0||version<=0)throw WorkOrderError(422,"VALIDATION_ERROR","Expected positive int64");
    if(type!="ARRIVED"&&type!="PROGRESS_NOTE")throw WorkOrderError(422,"VALIDATION_ERROR","Invalid progress type","type");
    SqliteTransaction tx(db_);workflow::authenticate(db_,user,token);AuthorizationService auth(db_);workflow::require(auth.operation(user,Action::Progress,id,version));
    const auto order=WorkOrderRepository(db_).find(id);TransferRepository repo(db_);const auto a=repo.current(id);
    if(a.userId!=user)throw WorkOrderError(403,"ACTION_FORBIDDEN","Assignment changed");if(!a.acceptedAt)throw WorkOrderError(409,"ORDER_STATUS_CONFLICT","Task not accepted");
    const bool arrived=type=="ARRIVED";const auto event=arrived?ProcessEvent::Arrived:ProcessEvent::ProgressNote;
    if(!WorkOrderStateMachine::processAllowed(order.status,event))throw WorkOrderError(409,"ORDER_STATUS_CONFLICT","Event unavailable");
    if(!arrived&&!note)throw WorkOrderError(422,"VALIDATION_ERROR","Progress note required","note");if(note)workflow::note(*note,arrived?500:1000,"note",arrived);
    if(arrived&&repo.arrived(a.id))throw WorkOrderError(409,"DUPLICATE_OPERATION","Assignment arrival already recorded");
    const auto utc=orderTime(clock_()).utc;StatusLogEntry e;e.orderId=id;e.actorId=user;e.assignmentId=a.id;
    for(const auto &rule:WorkOrderStateMachine::processRules())if(rule.event==event)e.action=std::string(rule.logAction);
    e.fromStatus=order.status;e.toStatus=order.status;e.publicNote=note;e.createdAt=utc;e.requestId=rid;StatusLogRepository(db_).append(e);const auto logId=sqlite3_last_insert_rowid(db_.nativeHandle());
    workflow::authenticate(db_,user,token);workflow::require(auth.operation(user,Action::Progress,id,version));tx.commit();return {logId,id,a.id,order.version,type,order.status,utc};
}
}
