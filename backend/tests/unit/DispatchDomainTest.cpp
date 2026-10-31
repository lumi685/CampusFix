#include "dispatch/DispatchStrategy.h"
#include "models/WorkOrder.h"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <type_traits>
#include <utility>

using namespace campusfix;
using namespace campusfix::dispatch;
namespace {
int assertions = 0, cases = 0;
void check(bool value) {
    ++assertions;
    if (!value) throw std::runtime_error("Dispatch assertion " + std::to_string(assertions) + " failed");
}
template<class F> void invalid(F action) {
    try { action(); } catch (const std::invalid_argument &) { check(true); return; }
    throw std::runtime_error("Expected invalid_argument");
}
template<class F> void test(const char *name, F action) {
    action(); ++cases; std::cout << "PASS " << name << '\n';
}
DispatchWorkOrder order() {
    DispatchWorkOrder result;
    result.id = 9000000001LL; result.version = 4; result.categoryId = 3; result.locationId = 44;
    result.status = "PENDING_DISPATCH"; result.approvedUrgency = "HIGH";
    result.requiredSkill = {8, "FIXTURE_SKILL", "Fictional skill", true};
    result.location = {{22, 11}, 11, 22}; return result;
}
DispatchWorker worker(BusinessId id = 4000000002LL) {
    DispatchWorker result;
    result.record.id = id; result.record.userId = id + 100;
    result.record.serviceAreaId = 22; result.record.capacity = 3; result.record.activeTasks = 1;
    result.record.active = true; result.record.availability = "AVAILABLE";
    result.record.skills = {{8, 4, true}}; result.accountRole = "worker"; result.accountStatus = "ACTIVE";
    result.serviceArea = {{11}, 11, 22}; return result;
}

// TEST ONLY: echo snapshots in explicit ID order, with a constructor-controlled
// synthetic exclusion. This is neither eligibility filtering nor production ranking.
class EchoFixtureStrategy final : public DispatchStrategy {
public:
    explicit EchoFixtureStrategy(BusinessId excluded = 0) : excluded_(excluded) {}
protected:
    DispatchResult doRecommend(const DispatchWorkOrder &, const std::vector<DispatchWorker> &input) const override {
        auto copy = input;
        std::sort(copy.begin(), copy.end(), [](const auto &a, const auto &b) { return a.record.id < b.record.id; });
        DispatchResult result;
        for (const auto &w : copy) {
            if (w.record.id == excluded_) result.excluded.push_back({w.record.id, {ExclusionReason::Unavailable}});
            else {
                DispatchCandidate c; c.workerId = w.record.id; c.activeTasks = w.record.activeTasks; c.capacity = w.record.capacity;
                c.reasons = {"TEST_ONLY snapshot echo; NOT an eligibility verdict"}; result.eligible.push_back(c);
            }
        }
        return result;
    }
private:
    BusinessId excluded_;
};
class RejectAllFixtureStrategy final : public DispatchStrategy {
protected:
    DispatchResult doRecommend(const DispatchWorkOrder &, const std::vector<DispatchWorker> &input) const override {
        DispatchResult result;
        for (const auto &w : input) result.excluded.push_back({w.record.id, {ExclusionReason::Unavailable}});
        return result;
    }
};
class MalformedFixtureStrategy final : public DispatchStrategy {
public:
    DispatchResult output;
protected:
    DispatchResult doRecommend(const DispatchWorkOrder &, const std::vector<DispatchWorker> &) const override { return output; }
};

// Include all input fields in a deterministic, pure test representation. No JSON/DB/hash libraries.
void append(std::ostream &out, const LocationAncestry &p) {
    for (auto id : p.ancestors) out << id << ',';
    out << '|' << p.campusId.value_or(0) << '|' << p.buildingId.value_or(0) << ';';
}
std::string snapshot(const DispatchWorkOrder &o, const std::vector<DispatchWorker> &workers) {
    std::ostringstream s;
    s << o.id << '|' << o.version << '|' << o.categoryId << '|' << o.locationId << '|' << o.status << '|' << o.approvedUrgency
      << '|' << o.requiredSkill.id << '|' << o.requiredSkill.code << '|' << o.requiredSkill.name << '|' << o.requiredSkill.requiresCertificate << '|' << o.categoryRequiresCertificate;
    append(s, o.location);
    for (const auto &w : workers) {
        const auto &r = w.record;
        s << r.id << '|' << r.userId << '|' << r.serviceAreaId << '|' << r.capacity << '|' << r.activeTasks << '|' << r.workerNo
          << '|' << r.displayName << '|' << r.availability << '|' << r.updatedAt << '|' << r.active << '|' << r.emergencyDuty;
        for (const auto &skill : r.skills) s << '|' << skill.skillId << '/' << skill.level << '/' << skill.certified;
        s << '|' << w.accountStatus << '|' << w.accountRole; append(s, w.serviceArea);
        s << '|' << w.history.efficiency.has_value() << '|' << w.history.rating.has_value();
        if (w.history.efficiency) for (const auto &x : *w.history.efficiency) s << '|' << x.assignmentId << '/' << x.targetCompletionMinutes << '/' << x.actualMinutes;
        if (w.history.rating) for (const auto &x : *w.history.rating) s << '|' << x.evaluationId << '/' << x.stars;
    }
    return s.str();
}
std::string resultSnapshot(const DispatchResult &r) {
    std::ostringstream s;
    for (const auto &c : r.eligible) { s << "E" << c.workerId << '/' << c.activeTasks << '/' << c.capacity << '/' << c.score.has_value() << '/' << c.rank.has_value(); for (const auto &reason : c.reasons) s << reason; }
    for (const auto &e : r.excluded) { s << "X" << e.workerId; for (auto reason : e.reasons) s << static_cast<int>(reason); }
    return s.str();
}
}

static_assert(std::is_abstract_v<DispatchStrategy> && std::has_virtual_destructor_v<DispatchStrategy>);
static_assert(std::is_same_v<BusinessId, std::int64_t> && sizeof(BusinessId) == 8);
static_assert(std::is_same_v<decltype(DispatchWorkOrder{}.id), decltype(WorkOrder{}.id)>);
static_assert(std::is_same_v<decltype(DispatchWorker{}.record), WorkerRecord>);
static_assert(std::is_same_v<decltype(DispatchWorkOrder{}.requiredSkill), SkillOption>);
static_assert(std::is_same_v<decltype(WorkerRecord{}.skills), std::vector<WorkerSkill>>);
static_assert(!std::is_same_v<DispatchCandidate, ExcludedWorker>);

int main() { try {
    test("safe defaults are incomplete, inactive and unscored", [] {
        const DispatchWorkOrder o; const DispatchWorker w; const DispatchCandidate c;
        check(o.id == 0 && o.version == 0 && o.status.empty() && o.approvedUrgency.empty());
        check(!w.record.active && !w.record.emergencyDuty && w.accountStatus.empty());
        check(!w.history.efficiency && !w.history.rating);
        check(!c.score && !c.rank && !c.efficiencySamples && !c.ratingSamples);
        invalid([&] { validateWorkOrder(o); }); invalid([&] { validateWorker(w); });
    });
    test("models construct and reuse existing business model types", [] {
        auto o = order(); auto w = worker(); validateWorkOrder(o); validateWorker(w);
        WorkOrder existing; existing.id = o.id; existing.version = o.version; existing.input.categoryId = o.categoryId;
        check(existing.id == o.id && existing.version == 4 && existing.input.categoryId == 3);
        check(w.record.skills.front().level == 4 && o.requiredSkill.requiresCertificate);
    });
    test("missing IDs, review status and reviewed urgency cannot be used", [] {
        for (const auto id : {BusinessId{0}, BusinessId{-1}}) {
            auto o = order(); o.id = id; invalid([&] { validateWorkOrder(o); });
            o = order(); o.categoryId = id; invalid([&] { validateWorkOrder(o); });
            o = order(); o.requiredSkill.id = id; invalid([&] { validateWorkOrder(o); });
            o = order(); o.locationId = id; invalid([&] { validateWorkOrder(o); });
            o = order(); o.version = id; invalid([&] { validateWorkOrder(o); });
        }
        for (const char *state : {"", "DRAFT", "PENDING_REVIEW", "CLOSED", "invented"}) {
            auto o = order(); o.status = state; invalid([&] { validateWorkOrder(o); });
        }
        for (const char *urgency : {"", "UNKNOWN"}) { auto o = order(); o.approvedUrgency = urgency; invalid([&] { validateWorkOrder(o); }); }
        for (const char *urgency : {"LOW", "NORMAL", "HIGH", "URGENT"}) { auto o = order(); o.approvedUrgency = urgency; validateWorkOrder(o); check(true); }
    });
    test("malformed worker data is rejected, without implementing eligibility", [] {
        for (const auto id : {BusinessId{0}, BusinessId{-1}}) {
            auto w = worker(); w.record.id = id; invalid([&] { validateWorker(w); });
            w = worker(); w.record.userId = id; invalid([&] { validateWorker(w); });
            w = worker(); w.record.serviceAreaId = id; invalid([&] { validateWorker(w); });
        }
        for (const auto capacity : {0, -1, 101}) { auto w = worker(); w.record.capacity = capacity; invalid([&] { validateWorker(w); }); }
        auto w = worker(); w.record.activeTasks = -1; invalid([&] { validateWorker(w); });
        w = worker(); w.record.availability = "invented"; invalid([&] { validateWorker(w); });
        w = worker(); w.accountRole.clear(); invalid([&] { validateWorker(w); });
        w = worker(); w.accountStatus.clear(); invalid([&] { validateWorker(w); });
        // These are representable snapshots; W6-D2 must decide exclusion later.
        w = worker(); w.record.active = false; w.accountStatus = "DISABLED"; w.record.availability = "OFF_DUTY";
        w.record.activeTasks = 8; w.record.skills.clear(); validateWorker(w); check(w.record.activeTasks > w.record.capacity);
    });
    test("skill level bounds, positive IDs and duplicates", [] {
        for (const auto level : {0, 6}) { auto w = worker(); w.record.skills[0].level = level; invalid([&] { validateWorker(w); }); }
        auto w = worker(); w.record.skills[0].skillId = 0; invalid([&] { validateWorker(w); });
        w = worker(); w.record.skills.push_back(w.record.skills[0]); invalid([&] { validateWorker(w); });
        for (const auto level : {1, 5}) { w = worker(); w.record.skills[0].level = level; validateWorker(w); check(true); }
    });
    test("location metadata cannot contradict its own ID path", [] {
        auto o = order(); o.location.ancestors = {11, 11}; invalid([&] { validateWorkOrder(o); });
        o = order(); o.location.ancestors.push_back(o.locationId); invalid([&] { validateWorkOrder(o); });
        o = order(); o.location.campusId = 99; invalid([&] { validateWorkOrder(o); });
        o = order(); o.location.buildingId = o.location.campusId; invalid([&] { validateWorkOrder(o); });
        auto w = worker(); w.serviceArea.ancestors = {-1}; invalid([&] { validateWorker(w); });
        o = order(); o.location = {}; validateWorkOrder(o); check(!o.location.campusId); // Missing is not a grant.
    });
    test("unknown history differs from verified zero history, no invented 60", [] {
        auto w = worker(); check(!w.history.efficiency && !w.history.rating);
        w.history.efficiency.emplace(); w.history.rating.emplace(); validateWorker(w);
        check(w.history.efficiency && w.history.efficiency->empty() && w.history.rating->empty());
        const auto r = EchoFixtureStrategy{}.recommend(order(), {w}); check(!r.eligible[0].score);
        w.history.efficiency = std::vector<EfficiencySample>{{1, 60, 0}, {2, 90, 15.5}};
        w.history.rating = std::vector<RatingSample>{{3, 1}, {4, 5}}; validateWorker(w); check(w.history.rating->size() == 2);
    });
    test("malformed raw statistics reject NaN, infinity, invalid stars and duplicates", [] {
        for (double minutes : {-1.0, std::numeric_limits<double>::infinity(), std::numeric_limits<double>::quiet_NaN()}) {
            auto w = worker(); w.history.efficiency = std::vector<EfficiencySample>{{1, 60, minutes}}; invalid([&] { validateWorker(w); });
        }
        auto w = worker(); w.history.efficiency = std::vector<EfficiencySample>{{0, 60, 2}}; invalid([&] { validateWorker(w); });
        w.history.efficiency = std::vector<EfficiencySample>{{1, 0, 2}}; invalid([&] { validateWorker(w); });
        w.history.efficiency = std::vector<EfficiencySample>{{1, 60, 2}, {1, 60, 2}}; invalid([&] { validateWorker(w); });
        for (const auto stars : {0, 6}) { w = worker(); w.history.rating = std::vector<RatingSample>{{1, stars}}; invalid([&] { validateWorker(w); }); }
        w = worker(); w.history.rating = std::vector<RatingSample>(21, {1, 4}); invalid([&] { validateWorker(w); });
        w.history.rating = std::vector<RatingSample>{{1, 4}, {1, 5}}; invalid([&] { validateWorker(w); });
        w.history.rating = std::vector<RatingSample>{{0, 4}}; invalid([&] { validateWorker(w); });
    });
    test("substitutable polymorphic test strategies", [] {
        std::unique_ptr<DispatchStrategy> strategy = std::make_unique<EchoFixtureStrategy>();
        check(strategy->recommend(order(), {worker()}).eligible.size() == 1);
        strategy = std::make_unique<RejectAllFixtureStrategy>();
        const auto result = strategy->recommend(order(), {worker()});
        check(result.eligible.empty() && result.excluded.size() == 1);
    });
    test("explicit fixture sorting is deterministic under repeats and permutations", [] {
        const EchoFixtureStrategy strategy(4000000003LL);
        std::vector<DispatchWorker> input{worker(4000000004LL), worker(4000000003LL), worker(4000000002LL)};
        const auto expected = strategy.recommend(order(), input);
        check(expected.eligible[0].workerId == 4000000002LL && expected.eligible[1].workerId == 4000000004LL);
        for (int n = 0; n < 5; ++n) check(resultSnapshot(strategy.recommend(order(), input)) == resultSnapshot(expected));
        std::reverse(input.begin(), input.end()); check(resultSnapshot(strategy.recommend(order(), input)) == resultSnapshot(expected));
        // NOT a test of production weighted scores or score/load/ID tie-breaking.
    });
    test("recommendation leaves complete input snapshots unchanged", [] {
        const auto o = order(); auto w = worker(); w.record.workerNo = "FIXTURE"; w.record.displayName = "Fictional";
        w.history.efficiency = std::vector<EfficiencySample>{{1, 30, 4.5}}; w.history.rating.emplace();
        const std::vector<DispatchWorker> input{w}; const auto before = snapshot(o, input);
        EchoFixtureStrategy{}.recommend(o, input); check(snapshot(o, input) == before);
    });
    test("empty workers produce an explicit empty result", [] {
        const auto result = EchoFixtureStrategy{}.recommend(order(), {});
        check(result.eligible.empty() && result.excluded.empty());
    });
    test("result partitions forbid duplicate, unknown and unexplained exclusions", [] {
        MalformedFixtureStrategy s; DispatchCandidate c; c.workerId = worker().record.id; c.capacity = 3;
        s.output.eligible = {c}; s.output.excluded = {{c.workerId, {ExclusionReason::AtCapacity}}};
        invalid([&] { s.recommend(order(), {worker()}); });
        s.output.eligible.clear(); s.output.excluded[0].reasons.clear(); invalid([&] { s.recommend(order(), {worker()}); });
        s.output.excluded[0].reasons = {ExclusionReason::Unknown}; invalid([&] { s.recommend(order(), {worker()}); });
        s.output.excluded[0].reasons = {static_cast<ExclusionReason>(999)}; invalid([&] { s.recommend(order(), {worker()}); });
        s.output.excluded = {{123, {ExclusionReason::AtCapacity}}}; invalid([&] { s.recommend(order(), {worker()}); });
        invalid([] { EchoFixtureStrategy{}.recommend(order(), {worker(), worker()}); });
    });
    test("score representation and fixed weights, no weighted algorithm claimed", [] {
        const auto w = worker(); DispatchResult result; DispatchCandidate c;
        c.workerId = w.record.id; c.capacity = 3; c.activeTasks = 1;
        check(!c.score); c.score = DispatchScore{{10, 20, 30, 40, 50, 60}, 42}; // Arbitrary TEST values, NOT a computed total.
        c.rank = 1; c.efficiencySamples = 0; c.ratingSamples = 2; result.eligible = {c}; validateResult(result, {w});
        check(result.eligible[0].score->components.skill == 10 && result.eligible[0].score->total == 42);
        const std::array<double, 6> expected{0.35, 0.20, 0.20, 0.10, 0.10, 0.05}; check(p0Weights == expected);
        double sum = 0; for (const auto weight : p0Weights) sum += weight; check(std::abs(sum - 1.0) < 1e-12);
        for (double bad : {-1.0, 100.1, std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::infinity()}) {
            result.eligible[0].score->total = bad; invalid([&] { validateResult(result, {w}); });
            result.eligible = {c}; result.eligible[0].score->components.skill = bad; invalid([&] { validateResult(result, {w}); }); result.eligible = {c};
        }
        result.eligible[0].rank = 0; invalid([&] { validateResult(result, {w}); });
        result.eligible = {c}; result.eligible[0].score.reset(); invalid([&] { validateResult(result, {w}); });
        result.eligible = {c}; result.eligible[0].ratingSamples = 21; invalid([&] { validateResult(result, {w}); });
    });
    test("64-bit identifiers survive input and polymorphic results without narrowing", [] {
        auto o = order(); o.id = std::numeric_limits<BusinessId>::max(); o.version = o.id;
        auto w = worker(); w.record.id = std::numeric_limits<BusinessId>::max();
        const auto r = EchoFixtureStrategy{}.recommend(o, {w}); check(r.eligible[0].workerId == w.record.id);
        check(o.id > std::numeric_limits<std::int32_t>::max());
    });
    std::cout << "PASS dispatch_domain: " << cases << " cases, " << assertions << " assertions; pure C++17, no I/O dependencies; fixture strategies only\n";
    return 0;
} catch (const std::exception &error) { std::cerr << error.what() << '\n'; return 1; } }
