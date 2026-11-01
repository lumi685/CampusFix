#include "services/WorkerTaskService.h"
#include "services/AuthService.h"
#include "services/AuthorizationService.h"
#include "repositories/WorkerTaskRepository.h"
#include "repositories/WorkOrderRepository.h"
#include "repositories/StatusLogRepository.h"
#include "domain/WorkOrderStateMachine.h"
#include "domain/WorkOrderValidation.h"
#include <limits>
namespace campusfix {
namespace {
void authenticate(SqliteConnection &db,std::int64_t user,const std::string &token) {
    try {if(AuthService::me(db,token).user.id!=user)throw std::invalid_argument("AUTH_INVALID");}
    catch(const std::invalid_argument &e){throw WorkOrderError(401,std::string(e.what())=="AUTH_EXPIRED"?"AUTH_EXPIRED":"AUTH_INVALID","Authentication failed");}
}
void validateReason(const std::string &value) {
    validateOrderText(value,500,"reason");bool content=false;
    for(std::size_t i=0;i<value.size();) {
        const auto lead=static_cast<unsigned char>(value[i++]);std::uint32_t cp=lead;int n=0;
        if(lead>=0xF0){cp=lead&7;n=3;}else if(lead>=0xE0){cp=lead&15;n=2;}else if(lead>=0xC0){cp=lead&31;n=1;}
        while(n-->0)cp=(cp<<6)|(static_cast<unsigned char>(value[i++])&63);
        if(!((cp>=9&&cp<=13)||cp==32||cp==0x85||cp==0xA0||cp==0x1680||(cp>=0x2000&&cp<=0x200A)||cp==0x2028||cp==0x2029||cp==0x202F||cp==0x205F||cp==0x3000))content=true;
    }
    if(!content)throw WorkOrderError(422,"VALIDATION_ERROR","Refusal reason required","reason");
}
}
WorkerTaskResult WorkerTaskService::perform(std::int64_t user,const std::string &token,std::int64_t id,std::int64_t version,
                                           const std::optional<std::string> &reason,const std::string &rid) {
    if(id<=0||version<=0)throw WorkOrderError(422,"VALIDATION_ERROR","Expected a positive integer");
    if(reason)validateReason(*reason);
    const bool refuse=reason.has_value();const auto action=refuse?Action::Refuse:Action::Accept;
    SqliteTransaction tx(db_);authenticate(db_,user,token);AuthorizationService authorization(db_);WorkerTaskRepository tasks(db_);
    auto permission=authorization.order(user,action,id);
    if(!permission.allowed()) {
        // Returning an already-completed operation conflict is not permission to
        // mutate a historical assignment. First require server-confirmed read scope.
        if(AuthorizationRepository(db_).subject(user).role=="worker"&&authorization.order(user,Action::Read,id).allowed()&&tasks.repeatedRefusal(id,user,version))
            throw WorkOrderError(409,"ORDER_STATUS_CONFLICT","Assignment already refused");
        throw WorkOrderError(permission.status,permission.code,"Task unavailable");
    }
    const auto order=WorkOrderRepository(db_).find(id);permission=authorization.operation(user,action,id,version);
    if(!permission.allowed()){WorkOrderError e(permission.status,permission.code,"Task state or version changed");if(permission.code=="ORDER_VERSION_CONFLICT")e.currentVersion=order.version;if(permission.code=="ORDER_STATUS_CONFLICT")e.currentStatus=order.status;throw e;}
    const auto a=tasks.current(id,user);
    if(a.acceptedAt)throw WorkOrderError(409,"ORDER_STATUS_CONFLICT","Assignment already accepted");
    if(order.version==std::numeric_limits<std::int64_t>::max()||a.version==std::numeric_limits<std::int64_t>::max())throw WorkOrderError(409,"ORDER_VERSION_CONFLICT","Version limit reached");
    const auto *rule=WorkOrderStateMachine::find(order.status,refuse?WorkflowAction::Refuse:WorkflowAction::Accept);
    if(!rule)throw WorkOrderError(409,"ORDER_STATUS_CONFLICT","Operation unavailable");
    const auto utc=orderTime(clock_()).utc;tasks.change(a,utc,refuse);if(refuse)tasks.supersede(id,a.id,utc);tasks.advance(id,version,std::string(rule->to),utc);
    StatusLogEntry log;log.orderId=id;log.actorId=user;log.assignmentId=a.id;log.action=refuse?"ORDER_REFUSED":"ORDER_ACCEPTED";log.fromStatus=order.status;log.toStatus=std::string(rule->to);log.createdAt=utc;log.requestId=rid;if(reason)log.publicNote=*reason;StatusLogRepository(db_).append(log);
    authenticate(db_,user,token);
    if(AuthorizationRepository(db_).subject(user).role!="worker"||!tasks.owner(a.id,user))throw WorkOrderError(403,"ACTION_FORBIDDEN","Assignment identity changed");
    tx.commit();return {id,version+1,std::string(rule->to),refuse?"":utc,refuse};
}
}
