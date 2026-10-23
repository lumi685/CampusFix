#include "services/WorkOrderService.h"
#include "services/AuthService.h"
#include "services/AuthorizationService.h"
#include "domain/WorkOrderValidation.h"
#include "domain/WorkOrderStateMachine.h"
#include "repositories/StatusLogRepository.h"

namespace campusfix {
namespace {
void require(const AccessDecision &d) {
    if(!d.allowed()) throw WorkOrderError(d.status,d.code,"Operation not permitted");
}
}
void WorkOrderService::authenticate(std::int64_t id,const std::string &token) {
    try {
        if(AuthService::me(db_,token).user.id!=id) throw WorkOrderError(401,"AUTH_INVALID","Invalid session");
    }catch(const std::invalid_argument &error) {throw WorkOrderError(401,error.what(),"Invalid session");}
}
WorkOrder WorkOrderService::create(std::int64_t id,const std::string &token,const WorkOrderInput &input,const std::string &requestId) {
    SqliteTransaction tx(db_);
    authenticate(id,token);
    require(AuthorizationService(db_).order(id,Action::Create,0));
    validateWorkOrderInput(input);
    const auto *rule=WorkOrderStateMachine::find(std::nullopt,WorkflowAction::Create);
    if(!rule) throw WorkOrderError(409,"ORDER_STATUS_CONFLICT","Creation is not allowed");
    WorkOrderRepository repo(db_);repo.validateReferences(input);
    const auto time=orderTime(clock_());
    const auto order=repo.create(id,input,time.utc);
    StatusLogEntry event;event.orderId=order.id;event.actorId=id;event.action="ORDER_CREATED";
    event.toStatus=rule->to;event.createdAt=time.utc;event.requestId=requestId;StatusLogRepository(db_).append(event);
    tx.commit();return order;
}
WorkOrder WorkOrderService::submit(std::int64_t user,const std::string &token,std::int64_t id,
                                   std::int64_t version,const std::string &requestId) {
    if(version<=0) throw WorkOrderError(422,"VALIDATION_ERROR","Expected a positive integer","expected_version");
    SqliteTransaction tx(db_);
    authenticate(user,token);
    const auto decision=AuthorizationService(db_).operation(user,Action::Submit,id,version);
    if(!decision.allowed()) {
        WorkOrderError error(decision.status,decision.code,"Operation not permitted");
        // A 409 is produced only after resource authorization succeeded.
        if(decision.status==409) {
            const auto current=WorkOrderRepository(db_).find(id);
            if(decision.code=="ORDER_VERSION_CONFLICT") error.currentVersion=current.version;
            if(decision.code=="ORDER_STATUS_CONFLICT") error.currentStatus=current.status;
        }
        throw error;
    }
    WorkOrderRepository repo(db_);const auto order=repo.find(id);
    const auto *rule=WorkOrderStateMachine::find(order.status,WorkflowAction::Submit);
    if(!rule) throw WorkOrderError(409,"ORDER_STATUS_CONFLICT","Submission is not allowed");
    validateWorkOrderInput(order.input);repo.validateReferences(order.input);
    const auto time=orderTime(clock_());
    if(order.status=="PENDING_SUPPLEMENT"&&(!order.orderNo||!order.submittedAt))
        throw WorkOrderError(500,"INTERNAL_ERROR","Submitted order history is incomplete");
    if(order.status=="DRAFT"&&(order.orderNo||order.submittedAt))
        throw WorkOrderError(500,"INTERNAL_ERROR","Draft numbering invariant violated");
    const auto number=order.orderNo?*order.orderNo:repo.allocateNumber(time.businessDate,time.utc);
    repo.submit(order,number,time.utc);
    StatusLogEntry event;event.orderId=id;event.actorId=user;event.action="ORDER_SUBMITTED";
    event.fromStatus=order.status;event.toStatus=rule->to;event.createdAt=time.utc;event.requestId=requestId;
    StatusLogRepository(db_).append(event);
    const auto result=repo.find(id);tx.commit();return result;
}
}
