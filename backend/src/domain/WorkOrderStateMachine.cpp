#include "domain/WorkOrderStateMachine.h"
#include <algorithm>

namespace campusfix {
const std::array<std::string_view,10> &WorkOrderStateMachine::states() {
    static constexpr std::array<std::string_view,10> values={"DRAFT","PENDING_REVIEW","PENDING_SUPPLEMENT","PENDING_DISPATCH",
        "PENDING_ACCEPT","IN_PROGRESS","PENDING_VERIFY","COMPLETED","CLOSED","CANCELLED"};return values;
}
const std::array<WorkflowRule,23> &WorkOrderStateMachine::rules() {
    using A=WorkflowAction;using E=WorkflowEffect;
    static constexpr std::array<WorkflowRule,23> values={{
        {"T-01",A::Create,std::nullopt,"DRAFT",E::Creation},
        {"T-02",A::Submit,"DRAFT","PENDING_REVIEW",E::StateChange},
        {"T-03",A::ApproveReview,"PENDING_REVIEW","PENDING_DISPATCH",E::StateChange},
        {"T-04",A::ReturnForSupplement,"PENDING_REVIEW","PENDING_SUPPLEMENT",E::StateChange},
        {"T-05",A::Submit,"PENDING_SUPPLEMENT","PENDING_REVIEW",E::StateChange},
        {"T-06",A::RejectReview,"PENDING_REVIEW","CANCELLED",E::StateChange},
        {"T-07",A::Cancel,"DRAFT","CANCELLED",E::StateChange},
        {"T-08",A::Cancel,"PENDING_REVIEW","CANCELLED",E::StateChange},
        {"T-09",A::Cancel,"PENDING_SUPPLEMENT","CANCELLED",E::StateChange},
        {"T-10",A::Recommend,"PENDING_DISPATCH","PENDING_DISPATCH",E::ReadOnly},
        {"T-11",A::Assign,"PENDING_DISPATCH","PENDING_ACCEPT",E::StateChange},
        {"T-12",A::Cancel,"PENDING_DISPATCH","CANCELLED",E::StateChange},
        {"T-13",A::Accept,"PENDING_ACCEPT","IN_PROGRESS",E::StateChange},
        {"T-14",A::Refuse,"PENDING_ACCEPT","PENDING_DISPATCH",E::StateChange},
        {"T-15",A::RequestTransfer,"PENDING_ACCEPT","PENDING_ACCEPT",E::SameStateEvent},
        {"T-16",A::RequestTransfer,"IN_PROGRESS","IN_PROGRESS",E::SameStateEvent},
        {"T-17",A::ApproveTransfer,"PENDING_ACCEPT","PENDING_DISPATCH",E::StateChange},
        {"T-18",A::ApproveTransfer,"IN_PROGRESS","PENDING_DISPATCH",E::StateChange},
        {"T-19",A::Complete,"IN_PROGRESS","PENDING_VERIFY",E::StateChange},
        {"T-20",A::VerifyPass,"PENDING_VERIFY","COMPLETED",E::StateChange},
        {"T-21",A::VerifyFail,"PENDING_VERIFY","IN_PROGRESS",E::StateChange},
        {"T-22",A::Evaluate,"COMPLETED","COMPLETED",E::SameStateEvent},
        {"T-23",A::Close,"COMPLETED","CLOSED",E::StateChange}
    }};return values;
}
const std::array<ProcessRule,2> &WorkOrderStateMachine::processRules() {
    static constexpr std::array<ProcessRule,2> values={{{"E-01","IN_PROGRESS","WORKER_ARRIVED",ProcessEvent::Arrived},
        {"E-02","IN_PROGRESS","WORKER_PROGRESS_NOTE",ProcessEvent::ProgressNote}}};return values;
}
bool WorkOrderStateMachine::knownState(std::string_view s) {return std::find(states().begin(),states().end(),s)!=states().end();}
const WorkflowRule *WorkOrderStateMachine::find(std::optional<std::string_view> from,WorkflowAction action) {
    for(const auto &rule:rules()) if(rule.from==from&&rule.action==action) return &rule;return nullptr;
}
bool WorkOrderStateMachine::processAllowed(std::string_view s,ProcessEvent event) {
    for(const auto &rule:processRules()) if(rule.event==event&&rule.state==s) return true;return false;
}
}
