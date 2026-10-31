#include "dispatch/EligibilityFilter.h"
#include <algorithm>
#include <set>
#include <stdexcept>
#include <utility>

namespace campusfix::dispatch {
namespace {
// validateWorkOrder/validateWorker already check positive IDs, duplicates,
// self-cycles and membership. This additional check is filter-specific: the
// verified, complete chain must run from nearest parent to the root campus.
// The pure function cannot independently prove that an adapter told the truth.
bool verifiedPath(BusinessId id, const LocationAncestry &path) {
    if (!path.verified || !path.campusId) return false;
    if (id == *path.campusId) return path.ancestors.empty() && !path.buildingId;
    if (path.ancestors.empty() || path.ancestors.back() != *path.campusId) return false;
    if (path.buildingId && *path.buildingId != id) {
        const auto building = std::find(path.ancestors.begin(), path.ancestors.end(), *path.buildingId);
        if (building == path.ancestors.end() || building == path.ancestors.end() - 1) return false;
    }
    return true;
}

std::optional<ExclusionReason> areaFailure(const DispatchWorkOrder &order, const DispatchWorker &worker) {
    const auto &area = worker.serviceArea;
    const auto &place = order.location;
    const auto areaId = worker.record.serviceAreaId;
    if (!verifiedPath(order.locationId, place) || !verifiedPath(areaId, area))
        return ExclusionReason::LocationUnverified;

    // P0 profile service areas are CAMPUS or BUILDING. Identity against typed
    // metadata is essential: sharing a campus does NOT widen a building scope.
    const bool campusScope = areaId == *area.campusId;
    const bool buildingScope = area.buildingId && areaId == *area.buildingId;
    if (!campusScope && !buildingScope) return ExclusionReason::LocationUnverified;
    if (place.buildingId && area.buildingId && place.buildingId == area.buildingId && place.campusId != area.campusId)
        return ExclusionReason::LocationUnverified; // Same building cannot belong to two campuses.
    if (place.campusId != area.campusId) return ExclusionReason::OutsideServiceArea;
    if (campusScope) return std::nullopt;
    if (!place.buildingId) return ExclusionReason::LocationUnverified;
    if (*place.buildingId != areaId) return ExclusionReason::OutsideServiceArea;
    return std::nullopt;
}
}

DispatchResult EligibilityFilter::filter(const DispatchWorkOrder &order,
                                         const std::vector<DispatchWorker> &workers) {
    validateWorkOrder(order);
    std::set<BusinessId> ids;
    std::vector<const DispatchWorker *> ordered;
    ordered.reserve(workers.size());
    for (const auto &worker : workers) {
        validateWorker(worker);
        if (!worker.skillsLoaded) throw std::invalid_argument("Dispatch worker skills not completely loaded");
        if (!ids.insert(worker.record.id).second) throw std::invalid_argument("Duplicate dispatch worker");
        ordered.push_back(&worker);
    }
    std::sort(ordered.begin(), ordered.end(), [](const auto *a, const auto *b) { return a->record.id < b->record.id; });

    DispatchResult result;
    for (const auto *worker : ordered) {
        const auto &record = worker->record;
        std::vector<ExclusionReason> failures;
        // Collect every independent, known fact in this fixed rule order.
        if (worker->accountStatus != "ACTIVE") failures.push_back(ExclusionReason::AccountInactive);
        if (worker->accountRole != "worker") failures.push_back(ExclusionReason::AccountRoleMismatch);
        if (!record.active) failures.push_back(ExclusionReason::WorkerInactive);
        if (record.availability != "AVAILABLE") failures.push_back(ExclusionReason::Unavailable);
        const auto skill = std::find_if(record.skills.begin(), record.skills.end(),
            [&](const auto &item) { return item.skillId == order.requiredSkill.id; });
        if (skill == record.skills.end()) failures.push_back(ExclusionReason::MissingSkill);
        else if ((order.categoryRequiresCertificate || order.requiredSkill.requiresCertificate) && !skill->certified)
            failures.push_back(ExclusionReason::MissingCertificate);
        if (const auto failure = areaFailure(order, *worker)) failures.push_back(*failure);
        if (record.activeTasks >= record.capacity) failures.push_back(ExclusionReason::AtCapacity);

        if (!failures.empty()) result.excluded.push_back({record.id, std::move(failures)});
        else {
            DispatchCandidate candidate;
            candidate.workerId = record.id;
            candidate.activeTasks = record.activeTasks;
            candidate.capacity = record.capacity;
            candidate.reasons = {"All P0 hard eligibility constraints satisfied"};
            result.eligible.push_back(std::move(candidate));
        }
    }
    // One pass emits exactly one entry for each input; no scoring or ranking.
    validateResult(result, workers);
    return result;
}
}
