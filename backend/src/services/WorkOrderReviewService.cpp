#include "services/WorkOrderService.h"
#include "services/AuthorizationService.h"
#include "domain/WorkOrderValidation.h"
#include "domain/WorkOrderStateMachine.h"
#include "repositories/ReviewRepository.h"
#include "repositories/StatusLogRepository.h"
#include <json/json.h>
#include <limits>

namespace campusfix {
namespace {
constexpr std::size_t internalNoteLimit=2000;
bool blankNote(const std::string &value) {
    // Same blank characters as the existing reason validator, after UTF-8 validation.
    for(std::size_t i=0;i<value.size();) {
        if(value[i]==' '||value[i]=='\t'||value[i]=='\r'||value[i]=='\n') ++i;
        else if(value.compare(i,3,"\xE3\x80\x80")==0) i+=3; // U+3000
        else return false;
    }
    return true;
}
void invalid(const char *field) {throw WorkOrderError(422,"VALIDATION_ERROR","Invalid review field",field);}
WorkflowAction validate(const ReviewInput &i) {
    if(i.expectedVersion<=0) invalid("expected_version");
    if(i.internalNote) validateOrderText(*i.internalNote,internalNoteLimit,"internal_note",true);
    if(i.action=="APPROVE") {
        if(!i.categoryId||*i.categoryId<=0) invalid("category_id");
        if(!i.approvedUrgency||(*i.approvedUrgency!="LOW"&&*i.approvedUrgency!="NORMAL"&&
           *i.approvedUrgency!="HIGH"&&*i.approvedUrgency!="URGENT")) invalid("approved_urgency");
        if(i.reason) invalid("reason");
        return WorkflowAction::ApproveReview;
    }
    if(i.action!="RETURN"&&i.action!="REJECT") invalid("action");
    if(i.categoryId) invalid("category_id");
    if(i.approvedUrgency) invalid("approved_urgency");
    if(!i.reason) invalid("reason");
    validateOrderText(*i.reason,500,"reason");
    return i.action=="RETURN"?WorkflowAction::ReturnForSupplement:WorkflowAction::RejectReview;
}
}
ReviewResult WorkOrderService::review(std::int64_t user,const std::string &token,std::int64_t id,
                                     const ReviewInput &input,const std::string &requestId) {
    if(id<=0) invalid("id");
    if(input.expectedVersion<=0) invalid("expected_version");
    SqliteTransaction tx(db_);
    // Do not trust the earlier Guard snapshot: revocations, ownership and scope are read again
    // under the same write transaction as the conditional update and append-only audit record.
    authenticate(user,token);
    const auto decision=AuthorizationService(db_).operation(user,Action::Review,id,input.expectedVersion);
    if(!decision.allowed()) {
        WorkOrderError error(decision.status,decision.code,"Operation not permitted");
        if(decision.status==409) {
            const auto current=WorkOrderRepository(db_).find(id);
            if(decision.code=="ORDER_VERSION_CONFLICT") error.currentVersion=current.version;
            if(decision.code=="ORDER_STATUS_CONFLICT") error.currentStatus=current.status;
        }
        throw error;
    }
    const auto order=WorkOrderRepository(db_).find(id);
    // SQLite promotes overflowing INTEGER addition to REAL; never store a non-integer version
    // or overflow the C++ response calculation, even for corrupted/extreme test data.
    if(order.version==std::numeric_limits<std::int64_t>::max()) {
        WorkOrderError error(409,"ORDER_VERSION_CONFLICT","Order version limit reached");
        error.currentVersion=order.version;throw error;
    }
    const auto action=validate(input);
    const auto *rule=WorkOrderStateMachine::find(order.status,action);
    if(!rule) throw WorkOrderError(409,"ORDER_STATUS_CONFLICT","Review is not allowed");
    ReviewRepository repository(db_);
    if(input.categoryId) repository.validateCategory(*input.categoryId);
    const auto utc=orderTime(clock_()).utc;
    repository.apply(id,input,std::string(rule->to),utc);
    StatusLogEntry event;event.orderId=id;event.actorId=user;event.fromStatus=order.status;
    event.toStatus=rule->to;event.createdAt=utc;event.requestId=requestId;
    event.action=input.action=="APPROVE"?"ORDER_REVIEWED":input.action=="RETURN"?"ORDER_RETURNED":"ORDER_REJECTED";
    event.publicNote=input.reason;event.internalNote=input.internalNote;
    if(event.internalNote&&blankNote(*event.internalNote)) event.internalNote.reset();
    Json::Value metadata(Json::objectValue);metadata["review_action"]=input.action;
    if(input.categoryId) metadata["category_id"]=Json::Int64(*input.categoryId);
    if(input.approvedUrgency) metadata["approved_urgency"]=*input.approvedUrgency;
    Json::StreamWriterBuilder writer;writer["indentation"]="";
    event.metadataJson=Json::writeString(writer,metadata);
    StatusLogRepository(db_).append(event);
    tx.commit();
    return {id,input.expectedVersion+1,input.action,std::string(rule->to)};
}
}
