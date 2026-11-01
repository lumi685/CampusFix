#include "services/VerificationService.h"
#include "services/WorkflowSupport.h"
#include "repositories/VerificationRepository.h"
#include "repositories/WorkOrderRepository.h"
#include "repositories/StatusLogRepository.h"
#include "domain/WorkOrderStateMachine.h"
#include <json/json.h>
#include <limits>
namespace campusfix {
VerificationResult VerificationService::verify(std::int64_t user,const std::string &token,std::int64_t id,std::int64_t version,bool approved,const std::optional<std::string> &reason,const std::string &rid) {
    if(id<=0||version<=0)throw WorkOrderError(422,"VALIDATION_ERROR","Expected positive int64");
    SqliteTransaction tx(db_);workflow::authenticate(db_,user,token);AuthorizationService auth(db_);const auto action=approved?Action::VerifyPass:Action::VerifyFail;
    workflow::require(auth.order(user,action,id));const auto order=WorkOrderRepository(db_).find(id);workflow::require(auth.operation(user,action,id,version));
    if(!approved&&!reason)throw WorkOrderError(422,"VALIDATION_ERROR","Rework reason required","reason");if(reason)workflow::note(*reason,500,"reason",approved);
    VerificationRepository repo(db_);const auto s=repo.read(id);workflow::note(s.summary,2000,"repair_summary");
    if(order.version==std::numeric_limits<std::int64_t>::max()||(approved&&s.assignmentVersion==std::numeric_limits<std::int64_t>::max())||(!approved&&s.reworks==std::numeric_limits<std::int64_t>::max()))throw WorkOrderError(409,"ORDER_VERSION_CONFLICT","Counter limit reached");
    const auto *rule=WorkOrderStateMachine::find(order.status,approved?WorkflowAction::VerifyPass:WorkflowAction::VerifyFail);
    if(!rule||!validUtcDateTime(s.accepted)||!validUtcDateTime(s.submitted))throw WorkOrderError(409,"ORDER_STATUS_CONFLICT","Repair result unavailable");
    const auto utc=orderTime(clock_()).utc;auto times=db_.prepare("SELECT julianday(?1)<=julianday(?2) AND julianday(?2)<=julianday(?3)");times.bindText(1,s.accepted);times.bindText(2,s.submitted);times.bindText(3,utc);times.step();
    if(times.columnIsNull(0)||times.columnInt(0)!=1)throw WorkOrderError(409,"ORDER_STATUS_CONFLICT","Invalid repair timestamps");
    repo.save(id,user,version,s,approved,utc);if(approved)repo.end(id,s,utc);
    StatusLogEntry e;e.orderId=id;e.actorId=user;e.assignmentId=s.assignment;e.action=approved?"ORDER_VERIFIED":"ORDER_REWORK_REQUESTED";e.fromStatus=order.status;e.toStatus=std::string(rule->to);e.publicNote=reason;e.createdAt=utc;e.requestId=rid;
    if(!approved){Json::Value metadata;metadata["rework_count"]=Json::Int64(s.reworks+1);Json::StreamWriterBuilder b;b["indentation"]="";e.metadataJson=Json::writeString(b,metadata);}
    StatusLogRepository(db_).append(e);if(sqlite3_changes(db_.nativeHandle())!=1)throw WorkOrderError(409,"ORDER_STATUS_CONFLICT","Verification audit not appended");
    workflow::authenticate(db_,user,token);workflow::require(auth.order(user,action,id));if(!repo.saved(id,user,version,s,approved,utc))throw WorkOrderError(409,"ORDER_STATUS_CONFLICT","Verification invariants changed");
    tx.commit();return {id,version+1,s.reworks+(approved?0:1),approved,std::string(rule->to)};
}
}
