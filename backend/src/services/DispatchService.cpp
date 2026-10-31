#include "services/DispatchService.h"
#include "services/AuthService.h"
#include "services/AuthorizationService.h"
#include "repositories/WorkOrderRepository.h"
#include "dispatch/WeightedScoreStrategy.h"
#include <map>
#include <stdexcept>

namespace campusfix {
void authenticateDispatch(SqliteConnection &db,std::int64_t user,const std::string &token) {
    try {
        if(AuthService::me(db,token).user.id!=user)throw std::invalid_argument("AUTH_INVALID");
    }catch(const std::invalid_argument &e) {
        throw WorkOrderError(401,std::string(e.what())=="AUTH_EXPIRED"?"AUTH_EXPIRED":"AUTH_INVALID","Authentication failed");
    }
}
std::string exclusionCode(dispatch::ExclusionReason reason) {
    using R=dispatch::ExclusionReason;
    switch(reason) {
    case R::AccountInactive:return "ACCOUNT_INACTIVE";
    case R::AccountRoleMismatch:return "ACCOUNT_ROLE_MISMATCH";
    case R::WorkerInactive:return "WORKER_INACTIVE";
    case R::Unavailable:return "UNAVAILABLE";
    case R::MissingSkill:return "MISSING_SKILL";
    case R::MissingCertificate:return "MISSING_CERTIFICATE";
    case R::OutsideServiceArea:return "OUTSIDE_SERVICE_AREA";
    case R::AtCapacity:return "AT_CAPACITY";
    case R::LocationUnverified:return "LOCATION_UNVERIFIED";
    default:throw std::logic_error("Unknown dispatch exclusion");
    }
}
Json::Value dispatchResponse(const DispatchData &data,const dispatch::DispatchResult &result) {
    Json::Value dto(Json::objectValue);dto["order_id"]=Json::Int64(data.order.id);dto["order_version"]=Json::Int64(data.order.version);
    dto["strategy"]="WEIGHTED_SCORE_V1";
    const char *keys[]={"skill","area","load","efficiency","rating","duty"};
    for(std::size_t i=0;i<dispatch::p0Weights.size();++i)dto["weights"][keys[i]]=dispatch::p0Weights[i];
    dto["top_candidates"]=Json::arrayValue;dto["other_eligible_candidates"]=Json::arrayValue;dto["excluded_summary"]=Json::objectValue;
    std::map<std::int64_t,std::string> names;for(const auto &worker:data.workers)names.emplace(worker.record.id,worker.record.displayName);
    for(const auto &c:result.eligible) {
        const auto &score=c.score.value();const auto &s=score.components;Json::Value row(Json::objectValue);
        row["rank"]=Json::UInt64(c.rank.value());row["worker_id"]=Json::Int64(c.workerId);row["worker_name"]=names.at(c.workerId);
        row["total_score"]=score.total;row["active_tasks"]=Json::Int64(c.activeTasks);row["max_concurrent_tasks"]=Json::Int64(c.capacity);
        const double parts[]={s.skill,s.area,s.load,s.efficiency,s.rating,s.duty};
        for(std::size_t i=0;i<6;++i)row["score_detail"][keys[i]]=parts[i];
        row["sample_counts"]["efficiency"]=Json::UInt64(c.efficiencySamples.value());row["sample_counts"]["rating"]=Json::UInt64(c.ratingSamples.value());
        std::string reason;for(const auto &part:c.reasons){if(!reason.empty())reason+="; ";reason+=part;}row["reason"]=reason;
        row["risks"]=Json::arrayValue;for(const auto &risk:c.risks)row["risks"].append(risk);
        dto[c.rank.value()<=3?"top_candidates":"other_eligible_candidates"].append(row);
    }
    for(const auto &excluded:result.excluded)for(const auto reason:excluded.reasons) {
        const auto key=exclusionCode(reason);const auto count=dto["excluded_summary"][key].isNull()?0:dto["excluded_summary"][key].asInt64();
        dto["excluded_summary"][key]=Json::Int64(count+1);
    }
    return dto;
}
Json::Value DispatchService::recommend(std::int64_t user,const std::string &token,std::int64_t id) {
    if(id<=0)throw WorkOrderError(422,"VALIDATION_ERROR","Invalid order id","id");
    SqliteTransaction tx(db_,false);authenticateDispatch(db_,user,token);
    AuthorizationService auth(db_);auto decision=auth.order(user,Action::Recommend,id);
    if(!decision.allowed())throw WorkOrderError(decision.status,decision.code,"Operation not permitted");
    const auto order=WorkOrderRepository(db_).find(id);decision=auth.operation(user,Action::Recommend,id,order.version);
    if(!decision.allowed()) {WorkOrderError e(decision.status,decision.code,"Recommendation not allowed");if(decision.status==409)e.currentStatus=order.status;throw e;}
    const auto data=DispatchRepository(db_).load(id);const auto result=dispatch::WeightedScoreStrategy{}.recommend(data.order,data.workers);
    auto dto=dispatchResponse(data,result);tx.commit();return dto;
}
}
