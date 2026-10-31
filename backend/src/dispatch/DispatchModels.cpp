#include "dispatch/DispatchModels.h"
#include <cmath>
#include <set>
#include <stdexcept>

namespace campusfix::dispatch {
namespace {
void require(bool condition, const char *message) {
    if (!condition) throw std::invalid_argument(message);
}
bool positive(BusinessId id) { return id > 0; }
void ancestry(BusinessId id, const LocationAncestry &path) {
    std::set<BusinessId> seen;
    for (const auto ancestor : path.ancestors)
        require(positive(ancestor) && ancestor != id && seen.insert(ancestor).second, "Invalid location ancestry");
    const auto belongs = [&](const auto value) { return !value || (positive(*value) && (*value == id || seen.count(*value))); };
    require(belongs(path.campusId) && belongs(path.buildingId), "Unrelated location metadata");
    require(!path.campusId || !path.buildingId || path.campusId != path.buildingId, "Conflicting location types");
}
bool scoreValue(double value) { return std::isfinite(value) && value >= 0 && value <= 100; }
void history(const DispatchHistory &data) {
    if (data.efficiency) {
        require(data.efficiency->size() <= 20, "Too many efficiency samples");
        std::set<BusinessId> ids;
        for (const auto &sample : *data.efficiency)
            require(positive(sample.assignmentId) && ids.insert(sample.assignmentId).second &&
                sample.targetCompletionMinutes > 0 && std::isfinite(sample.actualMinutes) && sample.actualMinutes >= 0,
                "Invalid efficiency sample");
    }
    if (data.rating) {
        require(data.rating->size() <= 20, "Too many rating samples");
        std::set<BusinessId> ids;
        for (const auto &sample : *data.rating)
            require(positive(sample.evaluationId) && ids.insert(sample.evaluationId).second &&
                sample.stars >= 1 && sample.stars <= 5, "Invalid rating sample");
    }
}
}
void validateWorkOrder(const DispatchWorkOrder &order) {
    require(positive(order.id) && positive(order.version) && positive(order.categoryId) &&
        positive(order.locationId) && positive(order.requiredSkill.id), "Incomplete dispatch order");
    require(order.status == "PENDING_DISPATCH", "Order is not pending dispatch");
    const auto &urgency = order.approvedUrgency;
    require(urgency == "LOW" || urgency == "NORMAL" || urgency == "HIGH" || urgency == "URGENT", "Missing reviewed urgency");
    ancestry(order.locationId, order.location);
}
void validateWorker(const DispatchWorker &worker) {
    const auto &record = worker.record;
    require(positive(record.id) && positive(record.userId) && positive(record.serviceAreaId), "Incomplete worker snapshot");
    require(record.activeTasks >= 0 && record.capacity > 0 && record.capacity <= 100, "Invalid load or capacity");
    const auto &availability = record.availability;
    require(availability == "AVAILABLE" || availability == "BUSY" || availability == "OFF_DUTY" || availability == "DISABLED", "Unknown availability");
    require(worker.accountStatus == "ACTIVE" || worker.accountStatus == "DISABLED", "Unknown account status");
    const auto &role = worker.accountRole;
    require(role == "reporter" || role == "worker" || role == "dispatcher" || role == "sysadmin", "Unknown account role");
    std::set<BusinessId> skills;
    for (const auto &skill : record.skills)
        require(positive(skill.skillId) && skills.insert(skill.skillId).second && skill.level >= 1 && skill.level <= 5,
            "Invalid or duplicate worker skill");
    ancestry(record.serviceAreaId, worker.serviceArea);
    history(worker.history);
    // Disabled/busy/missing skills/outside area/full or overloaded are real,
    // representable snapshots, NOT eligibility decisions made in W6-D1.
}
void validateResult(const DispatchResult &result, const std::vector<DispatchWorker> &workers) {
    std::set<BusinessId> available, seen;
    for (const auto &worker : workers) available.insert(worker.record.id);
    const auto unique = [&](BusinessId id) {
        require(positive(id) && available.count(id) && seen.insert(id).second, "Unknown or duplicate result worker");
    };
    for (const auto &candidate : result.eligible) {
        unique(candidate.workerId);
        require(candidate.activeTasks >= 0 && candidate.capacity > 0 && candidate.capacity <= 100, "Invalid candidate load");
        require(!candidate.rank || (*candidate.rank > 0 && candidate.score.has_value()), "Rank without a score");
        require(!candidate.efficiencySamples || *candidate.efficiencySamples <= 20, "Invalid efficiency sample count");
        require(!candidate.ratingSamples || *candidate.ratingSamples <= 20, "Invalid rating sample count");
        if (candidate.score) {
            const auto &s = *candidate.score;
            require(scoreValue(s.total) && scoreValue(s.components.skill) && scoreValue(s.components.area) &&
                scoreValue(s.components.load) && scoreValue(s.components.efficiency) && scoreValue(s.components.rating) &&
                scoreValue(s.components.duty), "Invalid candidate score");
        }
    }
    for (const auto &excluded : result.excluded) {
        unique(excluded.workerId);
        require(!excluded.reasons.empty(), "Excluded worker has no reason");
        for (const auto reason : excluded.reasons) {
            switch (reason) {
            case ExclusionReason::AccountInactive:
            case ExclusionReason::WorkerInactive:
            case ExclusionReason::Unavailable:
            case ExclusionReason::MissingSkill:
            case ExclusionReason::MissingCertificate:
            case ExclusionReason::OutsideServiceArea:
            case ExclusionReason::AtCapacity:
            case ExclusionReason::AccountRoleMismatch:
            case ExclusionReason::LocationUnverified:
                break;
            default:
                require(false, "Unknown exclusion reason");
            }
        }
    }
    // No weighted sum, eligibility verdict, ranking, or Top 3 computation here.
}
}
