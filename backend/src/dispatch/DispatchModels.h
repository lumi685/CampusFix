#pragma once
#include "models/Worker.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace campusfix::dispatch {
using BusinessId = std::int64_t;

// Derived location metadata. Ancestors exclude the node itself; no permission
// can be inferred from missing metadata. Repository mapping is future work.
struct LocationAncestry {
    std::vector<BusinessId> ancestors;
    std::optional<BusinessId> campusId, buildingId;
};

// A transient, server-loaded projection, NOT another persisted WorkOrder.
// categoryId is the reviewed work_orders.category_id; never a client hint.
struct DispatchWorkOrder {
    BusinessId id = 0, version = 0, categoryId = 0, locationId = 0;
    std::string status, approvedUrgency;
    SkillOption requiredSkill;
    bool categoryRequiresCertificate = false;
    LocationAncestry location;
};

// Raw statistics, not cold-start-adjusted scores. Selection of the latest 20
// valid samples belongs to the future repository adapter; formulas to W6-D3.
struct EfficiencySample {
    BusinessId assignmentId = 0;
    std::int64_t targetCompletionMinutes = 0;
    double actualMinutes = 0;
};
struct RatingSample { BusinessId evaluationId = 0; std::int64_t stars = 0; };
struct DispatchHistory {
    // nullopt = not loaded; an engaged empty vector = verified zero samples.
    std::optional<std::vector<EfficiencySample>> efficiency;
    std::optional<std::vector<RatingSample>> rating;
};
struct DispatchWorker {
    WorkerRecord record; // Reuse IDs, active/availability, skill levels, load and capacity.
    std::string accountStatus, accountRole; // users.status/role, not inferred from the profile.
    LocationAncestry serviceArea;
    DispatchHistory history;
};

// Fixed P0 order: skill, area, load, efficiency, rating, duty. No configurable weights.
inline constexpr std::array<double, 6> p0Weights{0.35, 0.20, 0.20, 0.10, 0.10, 0.05};
struct ScoreComponents {
    double skill = 0, area = 0, load = 0, efficiency = 0, rating = 0, duty = 0;
};
struct DispatchScore { ScoreComponents components; double total = 0; };
struct DispatchCandidate {
    BusinessId workerId = 0;
    std::int64_t activeTasks = 0, capacity = 0;
    std::optional<DispatchScore> score; // No score, rather than a fictitious zero/60.
    std::optional<std::size_t> rank;
    std::optional<std::size_t> efficiencySamples, ratingSamples;
    std::vector<std::string> reasons, risks;
};
enum class ExclusionReason {
    Unknown, AccountInactive, WorkerInactive, Unavailable, MissingSkill,
    MissingCertificate, OutsideServiceArea, AtCapacity
};
struct ExcludedWorker {
    BusinessId workerId = 0;
    std::vector<ExclusionReason> reasons;
    // Deliberately no score/rank: a high score cannot promote an excluded entry.
};
struct DispatchResult {
    // Future W6-D3 supplies ranked eligible candidates; API layer splits Top 3/rest.
    std::vector<DispatchCandidate> eligible;
    std::vector<ExcludedWorker> excluded;
};

// Structural/precondition validation only, NOT W6-D2 eligibility or W6-D3 scoring.
// Throws std::invalid_argument; no HTTP codes, persistence or network operations.
void validateWorkOrder(const DispatchWorkOrder &order);
void validateWorker(const DispatchWorker &worker);
void validateResult(const DispatchResult &result, const std::vector<DispatchWorker> &workers);
}
