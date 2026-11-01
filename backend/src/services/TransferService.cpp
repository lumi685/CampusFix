#include "services/TransferService.h"
#include "services/WorkflowSupport.h"
#include "repositories/WorkOrderRepository.h"
#include "repositories/StatusLogRepository.h"
#include "domain/WorkOrderStateMachine.h"
#include <json/json.h>
#include <limits>
namespace campusfix {
namespace {
void log(SqliteConnection &db,std::int64_t order,std::int64_t actor,std::int64_t assignment,std::int64_t request,const std::string &action,
         const std::string &from,const std::string &to,const std::string &note,const std::string &utc,const std::string &rid,const std::string &decision={}) {
    StatusLogEntry e;e.orderId=order;e.actorId=actor;e.assignmentId=assignment;e.action=action;e.fromStatus=from;e.toStatus=to;e.publicNote=note;e.createdAt=utc;e.requestId=rid;
    Json::Value metadata;metadata["transfer_request_id"]=Json::Int64(request);if(!decision.empty())metadata["decision"]=decision;Json::StreamWriterBuilder b;b["indentation"]="";e.metadataJson=Json::writeString(b,metadata);StatusLogRepository(db).append(e);
}
}
TransferCreated TransferService::create(std::int64_t user,const std::string &token,std::int64_t id,std::int64_t version,const std::string &reason,const std::string &rid) {
    if(id<=0||version<=0)throw WorkOrderError(422,"VALIDATION_ERROR","Expected positive int64");
    SqliteTransaction tx(db_);workflow::authenticate(db_,user,token);AuthorizationService auth(db_);workflow::require(auth.order(user,Action::Transfer,id));
    const auto order=WorkOrderRepository(db_).find(id);workflow::require(auth.operation(user,Action::Transfer,id,version));workflow::note(reason,500,"reason");
    const auto *rule=WorkOrderStateMachine::find(order.status,WorkflowAction::RequestTransfer);if(!rule||rule->effect!=WorkflowEffect::SameStateEvent)throw WorkOrderError(409,"ORDER_STATUS_CONFLICT","Transfer unavailable");
    TransferRepository repo(db_);const auto a=repo.current(id);if(a.userId!=user)throw WorkOrderError(403,"ACTION_FORBIDDEN","Assignment changed");
    if(const auto pending=repo.pending(a.id))throw TransferDuplicate(*pending);
    const auto utc=orderTime(clock_()).utc;const auto request=repo.insert(id,a.id,user,reason,order.version,utc);
    log(db_,id,user,a.id,request,"TRANSFER_REQUESTED",order.status,order.status,reason,utc,rid);
    workflow::authenticate(db_,user,token);workflow::require(auth.operation(user,Action::Transfer,id,order.version));tx.commit();return {request,id,order.status};
}
TransferDecision TransferService::decide(std::int64_t user,const std::string &token,std::int64_t id,std::int64_t request,std::int64_t version,
                                         const std::string &decision,const std::string &reason,const std::string &rid) {
    if(id<=0||request<=0||version<=0)throw WorkOrderError(422,"VALIDATION_ERROR","Expected positive int64");
    if(decision!="APPROVE"&&decision!="REJECT")throw WorkOrderError(422,"VALIDATION_ERROR","Invalid decision","decision");
    SqliteTransaction tx(db_);workflow::authenticate(db_,user,token);AuthorizationService auth(db_);
    // Permission only: Assign's operation state is PENDING_DISPATCH and must NOT
    // be reused here. T-17/T-18 below provide the independent state check.
    workflow::require(auth.order(user,Action::Assign,id));const auto order=WorkOrderRepository(db_).find(id);
    TransferRepository repo(db_);const auto item=repo.find(id,request);if(item.status!="PENDING")throw WorkOrderError(409,"DUPLICATE_OPERATION","Transfer already resolved");
    const auto *rule=WorkOrderStateMachine::find(order.status,WorkflowAction::ApproveTransfer);if(!rule)throw WorkOrderError(409,"ORDER_STATUS_CONFLICT","Transfer decision unavailable");
    workflow::version(order,version);workflow::note(reason,500,"reason");const auto a=repo.current(id);
    if(item.assignmentId!=a.id)throw WorkOrderError(409,"ORDER_STATUS_CONFLICT","Transfer belongs to an old assignment");
    const bool approve=decision=="APPROVE";
    if(approve&&(order.version==std::numeric_limits<std::int64_t>::max()||a.version==std::numeric_limits<std::int64_t>::max()))throw WorkOrderError(409,"ORDER_VERSION_CONFLICT","Version limit reached");
    const auto utc=orderTime(clock_()).utc;repo.resolve(request,a.id,user,decision,reason,utc);
    const std::string target=approve?std::string(rule->to):order.status;
    if(approve){repo.end(a,utc);repo.advance(id,version,order.status,target,utc);}
    log(db_,id,user,a.id,request,approve?"TRANSFER_APPROVED":"TRANSFER_REJECTED",order.status,target,reason,utc,rid,decision);
    workflow::authenticate(db_,user,token);workflow::require(auth.order(user,Action::Assign,id));tx.commit();return {request,version+(approve?1:0),decision,target,approve};
}
}
