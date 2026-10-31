#include "services/AssignmentService.h"
#include "services/AuthorizationService.h"
#include "repositories/AssignmentRepository.h"
#include "repositories/WorkOrderRepository.h"
#include "repositories/StatusLogRepository.h"
#include "domain/WorkOrderStateMachine.h"
#include "domain/WorkOrderValidation.h"
#include "dispatch/WeightedScoreStrategy.h"
#include <algorithm>
#include <limits>

namespace campusfix {
namespace {
std::string json(const Json::Value &value) {Json::StreamWriterBuilder b;b["indentation"]="";return Json::writeString(b,value);}
bool blank(const std::string &s) {
    // validateOrderText has already verified UTF-8. Treat Unicode White_Space
    // characters as blank as well, so NBSP/thin-space cannot bypass Top 4 reason.
    for(std::size_t i=0;i<s.size();) {
        const auto lead=static_cast<unsigned char>(s[i++]);std::uint32_t cp=lead;int count=0;
        if(lead>=0xF0){cp=lead&7;count=3;}else if(lead>=0xE0){cp=lead&15;count=2;}else if(lead>=0xC0){cp=lead&31;count=1;}
        for(int n=0;n<count;++n)cp=(cp<<6)|(static_cast<unsigned char>(s[i++])&63);
        if(!((cp>=9&&cp<=13)||cp==32||cp==0x85||cp==0xA0||cp==0x1680||(cp>=0x2000&&cp<=0x200A)||
             cp==0x2028||cp==0x2029||cp==0x202F||cp==0x205F||cp==0x3000))return false;
    }
    return true;
}
}
AssignmentResult AssignmentService::assign(std::int64_t user,const std::string &token,std::int64_t id,
                                           const AssignmentInput &input,const std::string &requestId) {
    if(id<=0||input.workerId<=0||input.expectedVersion<=0)throw WorkOrderError(422,"VALIDATION_ERROR","Invalid assignment field");
    if(input.reason)validateOrderText(*input.reason,500,"reason",true);
    try {
        SqliteTransaction tx(db_);authenticateDispatch(db_,user,token);
        AuthorizationService auth(db_);auto decision=auth.order(user,Action::Assign,id);
        if(!decision.allowed())throw WorkOrderError(decision.status,decision.code,"Operation not permitted");
        AssignmentRepository repository(db_);
        if(repository.hasActive(id))throw WorkOrderError(409,"ACTIVE_ASSIGNMENT_CONFLICT","Order already assigned");
        const auto order=WorkOrderRepository(db_).find(id);decision=auth.operation(user,Action::Assign,id,input.expectedVersion);
        if(!decision.allowed()) {
            WorkOrderError e(decision.status,decision.code,"Assignment not allowed");
            if(decision.code=="ORDER_VERSION_CONFLICT")e.currentVersion=order.version;
            if(decision.code=="ORDER_STATUS_CONFLICT")e.currentStatus=order.status;throw e;
        }
        if(order.version==std::numeric_limits<std::int64_t>::max())throw WorkOrderError(409,"ORDER_VERSION_CONFLICT","Order version limit reached");
        const auto *rule=WorkOrderStateMachine::find(order.status,WorkflowAction::Assign);
        if(!rule)throw WorkOrderError(409,"ORDER_STATUS_CONFLICT","Assignment not allowed");
        const auto data=DispatchRepository(db_).load(id);const auto result=dispatch::WeightedScoreStrategy{}.recommend(data.order,data.workers);
        const auto selected=std::find_if(result.eligible.begin(),result.eligible.end(),[&](const auto &c){return c.workerId==input.workerId;});
        if(selected==result.eligible.end()) {
            const auto excluded=std::find_if(result.excluded.begin(),result.excluded.end(),[&](const auto &e){return e.workerId==input.workerId;});
            throw DispatchError(422,"WORKER_INELIGIBLE","Selected worker is not eligible",excluded==result.excluded.end()?"":exclusionCode(excluded->reasons.front()));
        }
        auto reason=input.reason;if(reason&&blank(*reason))reason.reset();
        if(selected->rank.value()>3&&!reason)throw WorkOrderError(422,"VALIDATION_ERROR","A reason is required outside Top 3","reason");
        const auto worker=std::find_if(data.workers.begin(),data.workers.end(),[&](const auto &w){return w.record.id==input.workerId;});
        const auto utc=orderTime(clock_()).utc;const auto dto=dispatchResponse(data,result);
        const auto assignment=repository.insert(id,input.workerId,user,reason,utc);
        Json::Value candidates=dto["top_candidates"];for(const auto &c:dto["other_eligible_candidates"])candidates.append(c);
        repository.snapshot(id,input.workerId,user,assignment,order.version,json(dto["weights"]),json(candidates),json(dto["excluded_summary"]),utc);
        repository.advance(id,order.version,std::string(rule->to),utc);
        StatusLogEntry log;log.orderId=id;log.actorId=user;log.assignmentId=assignment;log.action="ORDER_ASSIGNED";
        log.fromStatus=order.status;log.toStatus=std::string(rule->to);log.createdAt=utc;log.requestId=requestId;
        Json::Value metadata(Json::objectValue);metadata["assignment_id"]=Json::Int64(assignment);metadata["worker_id"]=Json::Int64(input.workerId);
        metadata["candidate_rank"]=Json::UInt64(selected->rank.value());if(reason)metadata["assign_reason"]=*reason;
        log.metadataJson=json(metadata);StatusLogRepository(db_).append(log);
        // Also fail closed for in-transaction trigger changes; external writers
        // cannot commit a revocation through this connection's IMMEDIATE lock.
        authenticateDispatch(db_,user,token);decision=auth.order(user,Action::Assign,id);
        if(!decision.allowed())throw WorkOrderError(decision.status,decision.code,"Operation not permitted");
        tx.commit();return {id,assignment,input.workerId,order.version+1,worker->record.displayName,std::string(rule->to)};
    }catch(const SqliteError &e) {
        if(e.primaryCode()==SQLITE_CONSTRAINT&&std::string(e.what()).find("UNIQUE constraint failed: assignments.order_id")!=std::string::npos)
            throw WorkOrderError(409,"ACTIVE_ASSIGNMENT_CONFLICT","Order already assigned");
        throw;
    }
}
}
