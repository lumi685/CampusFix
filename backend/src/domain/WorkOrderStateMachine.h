#pragma once
#include <array>
#include <optional>
#include <string_view>

namespace campusfix {
enum class WorkflowAction {
    Create,Submit,ApproveReview,ReturnForSupplement,RejectReview,Cancel,Recommend,Assign,
    Accept,Refuse,RequestTransfer,ApproveTransfer,Complete,VerifyPass,VerifyFail,Evaluate,Close
};
enum class WorkflowEffect { Creation,StateChange,ReadOnly,SameStateEvent };
struct WorkflowRule {
    std::string_view id;
    WorkflowAction action;
    std::optional<std::string_view> from;
    std::string_view to;
    WorkflowEffect effect;
};
enum class ProcessEvent { Arrived,ProgressNote };
struct ProcessRule {std::string_view id,state,logAction;ProcessEvent event;};
// Pure definitions, not authorization and not a database mutation API.
class WorkOrderStateMachine {
public:
    static const std::array<std::string_view,10> &states();
    static const std::array<WorkflowRule,23> &rules();
    static const std::array<ProcessRule,2> &processRules();
    static bool knownState(std::string_view status);
    static const WorkflowRule *find(std::optional<std::string_view> from,WorkflowAction action);
    static bool processAllowed(std::string_view status,ProcessEvent event);
};
}
