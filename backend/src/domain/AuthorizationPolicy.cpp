#include "domain/AuthorizationPolicy.h"
#include "domain/WorkOrderStateMachine.h"
#include <algorithm>
#include <set>

namespace campusfix {
namespace {
AccessDecision denied(int status) {
    return {status,status==401?"AUTH_INVALID":status==404?"RESOURCE_NOT_FOUND":"ACTION_FORBIDDEN"};
}
bool managed(const Subject &s,const char *cap,const OrderScope &o) {
    if(s.role=="dispatcher") return AuthorizationPolicy::capability(s,"AREA_SCOPE",o.ancestors);
    return s.role=="sysadmin" && AuthorizationPolicy::capability(s,cap,o.ancestors);
}
}
bool AuthorizationPolicy::capability(const Subject &s,const std::string &cap,
                                    const std::vector<std::int64_t> &ancestors) {
    if(!s.active || ancestors.empty()) return false;
    for(const auto &g:s.grants)
        if(g.capability==cap && (g.global || std::find(ancestors.begin(),ancestors.end(),g.locationId)!=ancestors.end())) return true;
    return false;
}
bool AuthorizationPolicy::originalReporter(const Subject &s,const OrderScope &o) {
    return s.active && (s.role=="reporter" || s.role=="worker") && s.id==o.reporterId;
}
bool AuthorizationPolicy::mayRead(const Subject &s,const OrderScope &o) {
    return s.active && o.exists && (originalReporter(s,o) ||
        (s.role=="worker" && (o.currentWorker || o.historicalWorker)) || managed(s,"BUSINESS_READ",o));
}
AccessDecision AuthorizationPolicy::order(const Subject &s,Action a,const OrderScope &o) {
    if(!s.active || s.id<=0) return denied(401);
    if(a==Action::Create) return s.role=="reporter"||s.role=="worker"?AccessDecision{}:denied(403);
    if(!o.exists) return denied(404);
    bool allow=false;
    switch(a) {
    case Action::Read: allow=mayRead(s,o);break;
    case Action::Edit: case Action::Submit: case Action::VerifyPass: case Action::VerifyFail:
    case Action::Evaluate: allow=originalReporter(s,o);break;
    case Action::Cancel: allow=originalReporter(s,o)||managed(s,"CANCEL",o);break;
    case Action::Review: allow=managed(s,"REVIEW",o);break;
    case Action::Recommend: case Action::Assign: allow=managed(s,"DISPATCH",o);break;
    case Action::Accept: case Action::Refuse: case Action::Transfer:
    case Action::Progress: case Action::Complete: allow=s.role=="worker"&&o.currentWorker;break;
    case Action::Close: allow=managed(s,"CLOSE",o);break;
    case Action::InternalNotes:
        allow=mayRead(s,o) && managed(s,"INTERNAL_NOTE_READ",o);break;
    default: break;
    }
    if(allow) return {};
    return denied(mayRead(s,o)?403:404);
}
AccessDecision AuthorizationPolicy::operation(const Subject &s,Action a,const OrderScope &o,
                                             std::int64_t version) {
    const auto permission=order(s,a,o); if(!permission.allowed()) return permission;
    bool valid=false;
    const auto flow=[&](WorkflowAction action){return WorkOrderStateMachine::find(o.status,action)!=nullptr;};
    switch(a) {
    case Action::VerifyPass:valid=flow(WorkflowAction::VerifyPass);break;
    case Action::VerifyFail:valid=flow(WorkflowAction::VerifyFail);break;
    case Action::Evaluate:valid=flow(WorkflowAction::Evaluate);break;
    case Action::Edit:valid=o.status=="DRAFT"||o.status=="PENDING_SUPPLEMENT";break; // Content edit, not a T transition.
    case Action::Submit:valid=flow(WorkflowAction::Submit);break;
    case Action::Review:valid=flow(WorkflowAction::ApproveReview);break;
    case Action::Recommend:valid=flow(WorkflowAction::Recommend);break;
    case Action::Assign:valid=flow(WorkflowAction::Assign);break;
    case Action::Accept:valid=flow(WorkflowAction::Accept);break;
    case Action::Refuse:valid=flow(WorkflowAction::Refuse);break;
    case Action::Transfer:valid=flow(WorkflowAction::RequestTransfer);break;
    case Action::Complete:valid=flow(WorkflowAction::Complete);break;
    case Action::Progress:valid=WorkOrderStateMachine::processAllowed(o.status,ProcessEvent::ProgressNote);break;
    case Action::Close:valid=flow(WorkflowAction::Close);break;
    case Action::Cancel:
        valid=flow(WorkflowAction::Cancel)&&(o.status!="PENDING_DISPATCH"||managed(s,"CANCEL",o));break;
    default: return {403,"ACTION_FORBIDDEN"};
    }
    if(!valid) return {409,"ORDER_STATUS_CONFLICT"};
    if(version<=0 || version!=o.version) return {409,"ORDER_VERSION_CONFLICT"};
    if(a==Action::Evaluate && o.evaluated) return {409,"DUPLICATE_OPERATION"};
    return {};
}
AccessDecision AuthorizationPolicy::location(const Subject &s,Action a,
                 const std::vector<std::int64_t> &ancestors,bool ownWorker) {
    if(!s.active || s.id<=0) return denied(401);
    if(ancestors.empty()) return denied(404);
    if(a==Action::WorkerRead && s.role=="worker"&&ownWorker) return {};
    const char *cap=nullptr;
    if(a==Action::WorkerRead || a==Action::WorkerManage) cap="WORKER_MANAGE";
    if(a==Action::Statistics) cap="STATS_VIEW";
    if(!cap) return denied(403);
    if((s.role=="dispatcher"&&capability(s,"AREA_SCOPE",ancestors)) ||
       (s.role=="sysadmin"&&capability(s,cap,ancestors))) return {};
    return denied(403);
}
bool FieldVisibility::allows(const std::string &field) const {
    static const std::set<std::string> publicFields={"id","order_no","title","category","location",
        "requested_urgency","approved_urgency","status","version","created_at","submitted_at","updated_at",
        "action","from_status","to_status","public_note","actor","attachments"};
    if(publicInfo && publicFields.count(field)) return true;
    if(contact && (field=="contact_name"||field=="contact_phone"||field=="description"||field=="expected_visit_at")) return true;
    return internal && field=="internal_note";
}
}
