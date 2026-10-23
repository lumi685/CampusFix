#include "services/WorkOrderService.h"
#include "services/AuthorizationService.h"
#include "repositories/StatusLogRepository.h"
#include <json/json.h>
#include <limits>
#include <memory>

namespace campusfix {
void validateTimelineQuery(const TimelineQuery &q) {
    if(q.page<1) throw WorkOrderError(422,"VALIDATION_ERROR","Invalid timeline page","page");
    if(q.pageSize<1||q.pageSize>100) throw WorkOrderError(422,"VALIDATION_ERROR","Invalid timeline page size","page_size");
    if(q.page-1>std::numeric_limits<std::int64_t>::max()/q.pageSize)
        throw WorkOrderError(422,"VALIDATION_ERROR","Timeline offset too large","page");
}
namespace {
void projectManagement(TimelineItem &item,const StatusLogRecord &row) {
    if(!row.metadataJson||row.metadataJson->size()>65536) return;
    Json::CharReaderBuilder builder;builder["collectComments"]=false;builder["rejectDupKeys"]=true;
    builder["failIfExtra"]=true;builder["stackLimit"]=32;
    const std::unique_ptr<Json::CharReader> reader(builder.newCharReader());Json::Value metadata;std::string errors;
    const auto &raw=*row.metadataJson;
    try {if(!reader->parse(raw.data(),raw.data()+raw.size(),&metadata,&errors)||!metadata.isObject()) return;}
    catch(const Json::Exception &) {return;} // Corrupt/deep historical metadata fails closed, not the whole timeline.
    auto number=[&](const char *key,std::int64_t min=1) {
        const auto &v=metadata[key];
        if((v.type()==Json::intValue||v.type()==Json::uintValue)&&v.isInt64()&&v.asInt64()>=min)
            item.managementDetail[key]=v.asInt64();
    };
    auto text=[&](const char *key) {
        const auto &v=metadata[key];
        if(v.isString()) {const auto value=v.asString();if(value.size()<=2000&&value.find('\0')==std::string::npos) item.managementDetail[key]=value;}
    };
    const auto from=row.fromStatus.value_or("");
    const bool review=from=="PENDING_REVIEW"&&
        ((row.action=="ORDER_REVIEWED"&&row.toStatus=="PENDING_DISPATCH")||
         (row.action=="ORDER_RETURNED"&&row.toStatus=="PENDING_SUPPLEMENT")||
         (row.action=="ORDER_REJECTED"&&row.toStatus=="CANCELLED"));
    if(review) {
        const auto expected=row.toStatus=="PENDING_DISPATCH"?"APPROVE":row.toStatus=="PENDING_SUPPLEMENT"?"RETURN":"REJECT";
        if(metadata["review_action"].isString()&&metadata["review_action"].asString()==expected) text("review_action");
        number("category_id");
        const auto &urgency=metadata["approved_urgency"];
        if(urgency.isString()&&(urgency=="LOW"||urgency=="NORMAL"||urgency=="HIGH"||urgency=="URGENT")) text("approved_urgency");
    } else if(row.action=="ORDER_ASSIGNED"&&from=="PENDING_DISPATCH"&&row.toStatus=="PENDING_ACCEPT") {
        number("assignment_id");number("worker_id");number("candidate_rank");text("assign_reason");
    } else if((from=="PENDING_ACCEPT"||from=="IN_PROGRESS")&&
        ((row.action=="TRANSFER_REQUESTED"&&row.toStatus==from)||
         (row.action=="TRANSFER_APPROVED"&&row.toStatus=="PENDING_DISPATCH")||
         (row.action=="TRANSFER_REJECTED"&&row.toStatus==from))) {
        number("transfer_request_id");
        if(metadata["decision"].isString()&&
           ((row.action=="TRANSFER_APPROVED"&&metadata["decision"]=="APPROVE")||
            (row.action=="TRANSFER_REJECTED"&&metadata["decision"]=="REJECT"))) text("decision");
    } else if(row.action=="ORDER_REWORK_REQUESTED"&&from=="PENDING_VERIFY"&&row.toStatus=="IN_PROGRESS") {
        number("rework_count",0);
    }
    // All other event types and keys fail closed, including nested/raw JSON values.
}
}
TimelinePage WorkOrderService::timeline(std::int64_t user,const std::string &token,std::int64_t id,const TimelineQuery &query) {
    SqliteTransaction snapshot(db_,false);authenticate(user,token);
    if(id<=0) throw WorkOrderError(422,"VALIDATION_ERROR","Expected a positive integer","id");
    AuthorizationService authorization(db_);const auto decision=authorization.order(user,Action::Read,id);
    if(!decision.allowed()) throw WorkOrderError(decision.status,decision.code,"Work order not found");
    validateTimelineQuery(query);
    const bool internal=authorization.fields(user,id).internal;
    const auto records=StatusLogRepository(db_).page(id,query,internal);
    TimelinePage result;result.page=query.page;result.pageSize=query.pageSize;result.total=records.total;
    for(const auto &row:records.items) {
        TimelineItem item;item.id=row.id;item.action=row.action;item.fromStatus=row.fromStatus;item.toStatus=row.toStatus;
        item.publicNote=row.publicNote;item.actorId=row.actorId;item.actorName=row.actorName;item.createdAt=row.createdAt;
        if(internal) {item.internalNote=row.internalNote;projectManagement(item,row);}
        result.items.push_back(std::move(item));
    }
    snapshot.commit();return result;
}
}
