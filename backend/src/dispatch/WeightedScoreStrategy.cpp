#include "dispatch/WeightedScoreStrategy.h"
#include "dispatch/EligibilityFilter.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <map>
#include <stdexcept>

namespace campusfix::dispatch {
namespace {
double tenth(double value) {
    return std::round(std::clamp(value, 0.0, 100.0) * 10.0) / 10.0;
}
template<class Sample, class Score> double historyMean(const std::vector<Sample> &samples, Score score) {
    if (samples.size() < 3) return 60.0;
    double sum = 0;
    for (const auto &sample : samples) sum += score(sample); // Do NOT round individual samples.
    return tenth(sum / static_cast<double>(samples.size()));
}
double totalScore(const ScoreComponents &s) {
    const std::array<double, 6> values{s.skill, s.area, s.load, s.efficiency, s.rating, s.duty};
    // Components are already tenths. Derive integer percentages from the single
    // authoritative p0Weights; exact decimal half-up avoids 80.35 -> 80.3 due to
    // binary dot-product error. Bounds: sum <= 1000*100, safely within int64_t.
    std::int64_t weighted = 0;
    for (std::size_t i = 0; i < values.size(); ++i)
        weighted += std::llround(values[i] * 10.0) * std::llround(p0Weights[i] * 100.0);
    const auto totalTenths = (weighted + 50) / 100;
    return static_cast<double>(totalTenths) / 10.0;
}
void explainHistory(DispatchCandidate &candidate, const char *metric, std::size_t count) {
    const std::string name(metric);
    if (count < 3) {
        candidate.reasons.push_back(name + " cold start from " + std::to_string(count) + " samples");
        candidate.risks.push_back(name + " samples below 3; cold-start score 60");
    } else candidate.reasons.push_back(name + " mean from " + std::to_string(count) + " valid samples");
}
}

DispatchResult WeightedScoreStrategy::doRecommend(const DispatchWorkOrder &order,
                                                 const std::vector<DispatchWorker> &workers) const {
    auto result = EligibilityFilter::filter(order, workers);
    std::map<BusinessId, const DispatchWorker *> input;
    for (const auto &worker : workers) input.emplace(worker.record.id, &worker);
    // Missing history for an EXCLUDED worker is irrelevant; it is never scored.
    // Preflight ALL eligible histories before computing any score.
    for (const auto &candidate : result.eligible) {
        const auto &history = input.at(candidate.workerId)->history;
        if (!history.efficiency || !history.rating)
            throw std::invalid_argument("Eligible dispatch worker history not completely loaded");
    }

    for (auto &candidate : result.eligible) {
        const auto &worker = *input.at(candidate.workerId);
        const auto &record = worker.record;
        const auto skill = std::find_if(record.skills.begin(), record.skills.end(),
            [&](const auto &value) { return value.skillId == order.requiredSkill.id; });
        if (skill == record.skills.end()) throw std::logic_error("Eligibility filter returned a worker without required skill");
        const bool sameBuilding = order.location.buildingId && worker.serviceArea.buildingId &&
                                  order.location.buildingId == worker.serviceArea.buildingId;
        const bool duty = order.approvedUrgency == "URGENT" && record.emergencyDuty;
        ScoreComponents s;
        s.skill = static_cast<double>(skill->level) / 5.0 * 100.0;
        s.area = sameBuilding ? 100.0 : 70.0; // Only verified P0 eligible scope, never an unauthorized 30.
        s.load = tenth(100.0 * (1.0 - static_cast<double>(record.activeTasks) / static_cast<double>(record.capacity)));
        s.efficiency = historyMean(*worker.history.efficiency, [](const EfficiencySample &sample) {
            return std::clamp(100.0 * static_cast<double>(sample.targetCompletionMinutes) /
                              std::max(1.0, sample.actualMinutes), 0.0, 100.0);
        });
        s.rating = historyMean(*worker.history.rating, [](const RatingSample &sample) { return static_cast<double>(sample.stars) * 20.0; });
        s.duty = duty ? 100.0 : 0.0;
        candidate.score = DispatchScore{s, totalScore(s)};
        candidate.efficiencySamples = worker.history.efficiency->size();
        candidate.ratingSamples = worker.history.rating->size();
        candidate.reasons = {"Required skill level " + std::to_string(skill->level) + "/5",
                             sameBuilding ? "Verified building coverage" : "Verified campus coverage",
                             "Current load " + std::to_string(record.activeTasks) + "/" + std::to_string(record.capacity),
                             duty ? "Urgent on-duty bonus applied" : "No urgent on-duty bonus"};
        explainHistory(candidate, "Efficiency", *candidate.efficiencySamples);
        explainHistory(candidate, "Rating", *candidate.ratingSamples);
    }
    // Total is a canonical one-decimal value, so exact equality here is an
    // explicit business tie, not an epsilon comparator violating transitivity.
    std::sort(result.eligible.begin(), result.eligible.end(), [](const auto &a, const auto &b) {
        if (a.score->total != b.score->total) return a.score->total > b.score->total;
        if (a.activeTasks != b.activeTasks) return a.activeTasks < b.activeTasks;
        return a.workerId < b.workerId;
    });
    for (std::size_t i = 0; i < result.eligible.size(); ++i) result.eligible[i].rank = i + 1;
    return result; // No change to excluded; no Top 3 split or persistence.
}
}
