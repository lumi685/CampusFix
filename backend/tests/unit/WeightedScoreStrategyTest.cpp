#include "dispatch/WeightedScoreStrategy.h"
#include "dispatch/EligibilityFilter.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <limits>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <utility>

using namespace campusfix::dispatch;
namespace {
using Reason = ExclusionReason;
int cases = 0, assertions = 0;
void check(bool value) {
    ++assertions;
    if (!value) throw std::runtime_error("Weighted score assertion " + std::to_string(assertions) + " failed");
}
void near(double actual, double expected) { check(std::isfinite(actual) && std::abs(actual - expected) < 1e-9); }
template<class F> void test(const char *name, F action) { action(); ++cases; std::cout << "PASS " << name << '\n'; }
template<class F> void invalid(F action) {
    try { action(); } catch (const std::invalid_argument &) { check(true); return; }
    throw std::runtime_error("Expected invalid_argument");
}
DispatchWorkOrder order() {
    DispatchWorkOrder o; o.id = 9000000001LL; o.version = 2; o.categoryId = 3;
    o.status = "PENDING_DISPATCH"; o.approvedUrgency = "NORMAL";
    o.requiredSkill = {8, "FICTIONAL", "Fictional skill", false};
    o.locationId = 44; o.location = {{33, 22, 11}, 11, 22, true}; return o;
}
DispatchWorker worker(BusinessId id = 4000000001LL) {
    DispatchWorker w; w.record.id = id; w.record.userId = id; w.record.active = true;
    w.accountRole = "worker"; w.accountStatus = "ACTIVE"; w.record.availability = "AVAILABLE";
    w.record.serviceAreaId = 22; w.serviceArea = {{11}, 11, 22, true};
    w.record.capacity = 3; w.record.activeTasks = 0; w.record.skills = {{8, 5, false}}; w.skillsLoaded = true;
    w.history.efficiency.emplace(); w.history.rating.emplace(); return w;
}
DispatchCandidate single(const DispatchWorker &w, const DispatchWorkOrder &o = order()) {
    const auto r = WeightedScoreStrategy{}.recommend(o, {w});
    check(r.eligible.size() == 1 && r.excluded.empty());
    check(r.eligible.at(0).score.has_value() && r.eligible.at(0).rank == 1);
    return r.eligible.at(0);
}
bool has(const std::vector<std::string> &values, const std::string &value) {
    return std::find(values.begin(), values.end(), value) != values.end();
}
void efficiency(DispatchWorker &w, const std::vector<double> &minutes, std::int64_t target = 60) {
    w.history.efficiency.emplace(); BusinessId id = 1;
    for (double value : minutes) w.history.efficiency->push_back({id++, target, value});
}
void ratings(DispatchWorker &w, const std::vector<std::int64_t> &stars) {
    w.history.rating.emplace(); BusinessId id = 1;
    for (auto value : stars) w.history.rating->push_back({id++, value});
}
void sameExclusions(const DispatchResult &a, const DispatchResult &b) {
    check(a.excluded.size() == b.excluded.size());
    for (std::size_t i = 0; i < a.excluded.size(); ++i)
        check(a.excluded[i].workerId == b.excluded[i].workerId && a.excluded[i].reasons == b.excluded[i].reasons);
}
std::array<double, 6> parts(const DispatchScore &score) {
    const auto &s = score.components; return {s.skill, s.area, s.load, s.efficiency, s.rating, s.duty};
}
void consistent(const DispatchResult &r) {
    std::size_t rank = 1;
    for (const auto &c : r.eligible) {
        check(c.score && c.rank == rank++ && c.efficiencySamples && c.ratingSamples);
        double sum = 0; const auto values = parts(*c.score);
        for (std::size_t i = 0; i < values.size(); ++i) {
            check(std::isfinite(values[i]) && values[i] >= 0 && values[i] <= 100);
            near(values[i] * 10, std::round(values[i] * 10)); sum += values[i] * p0Weights[i];
        }
        // Independent decimal-display oracle; epsilon only compensates binary
        // representation in this test's dot product, NOT production ordering.
        near(c.score->total, std::floor(sum * 10 + 0.5 + 1e-9) / 10);
        check(c.score->total >= 0 && c.score->total <= 100);
    }
}
void append(std::ostream &s, const LocationAncestry &p) {
    for (auto id : p.ancestors) s << id << ',';
    s << '|' << p.campusId.value_or(0) << '|' << p.buildingId.value_or(0) << '|' << p.verified;
}
std::string inputSnapshot(const DispatchWorkOrder &o, const std::vector<DispatchWorker> &input) {
    std::ostringstream s; s << std::setprecision(17);
    s << o.id << '|' << o.version << '|' << o.categoryId << '|' << o.locationId << '|' << o.status << '|' << o.approvedUrgency
      << '|' << o.requiredSkill.id << '|' << std::quoted(o.requiredSkill.code) << '|' << std::quoted(o.requiredSkill.name)
      << '|' << o.requiredSkill.requiresCertificate << '|' << o.categoryRequiresCertificate; append(s, o.location);
    for (const auto &w : input) {
        const auto &r = w.record;
        s << '|' << r.id << '|' << r.userId << '|' << r.serviceAreaId << '|' << r.capacity << '|' << r.activeTasks
          << '|' << std::quoted(r.workerNo) << '|' << std::quoted(r.displayName) << '|' << r.availability << '|' << r.updatedAt
          << '|' << r.active << '|' << r.emergencyDuty << '|' << w.accountStatus << '|' << w.accountRole << '|' << w.skillsLoaded;
        append(s, w.serviceArea);
        for (const auto &x : r.skills) s << '|' << x.skillId << '/' << x.level << '/' << x.certified;
        s << '|' << w.history.efficiency.has_value() << '|' << w.history.rating.has_value();
        if (w.history.efficiency) for (const auto &x : *w.history.efficiency) s << '|' << x.assignmentId << '/' << x.targetCompletionMinutes << '/' << x.actualMinutes;
        if (w.history.rating) for (const auto &x : *w.history.rating) s << '|' << x.evaluationId << '/' << x.stars;
    }
    return s.str();
}
std::string resultSnapshot(const DispatchResult &r) {
    std::ostringstream s; s << std::setprecision(17);
    for (const auto &c : r.eligible) {
        s << "E" << c.workerId << '/' << c.activeTasks << '/' << c.capacity << '/' << c.rank.value_or(0)
          << '/' << c.efficiencySamples.value_or(99) << '/' << c.ratingSamples.value_or(99);
        if (c.score) { for (double value : parts(*c.score)) s << '/' << value; s << '/' << c.score->total; }
        for (const auto &x : c.reasons) s << std::quoted(x);
        for (const auto &x : c.risks) s << std::quoted(x);
    }
    for (const auto &e : r.excluded) { s << "X" << e.workerId; for (auto reason : e.reasons) s << '/' << static_cast<int>(reason); }
    return s.str();
}
void experiment(const char *name, const DispatchWorkOrder &o, const std::vector<DispatchWorker> &workers, const DispatchResult &r) {
    std::cout << "EXPERIMENT " << name << " urgency=" << o.approvedUrgency << '\n';
    for (const auto &w : workers)
        std::cout << " input worker=" << w.record.id << " required-level=" << (w.record.skills.empty() || w.record.skills[0].skillId != o.requiredSkill.id ? 0 : w.record.skills[0].level)
                  << " area=" << w.record.serviceAreaId << " load=" << w.record.activeTasks << '/' << w.record.capacity
                  << " duty=" << w.record.emergencyDuty << " samples=" << (w.history.efficiency ? std::to_string(w.history.efficiency->size()) : "unloaded")
                  << '/' << (w.history.rating ? std::to_string(w.history.rating->size()) : "unloaded") << '\n';
    for (const auto &c : r.eligible) {
        std::cout << " eligible=" << c.workerId << " rank=" << *c.rank << " parts=";
        for (double value : parts(*c.score)) std::cout << value << ',';
        std::cout << " total=" << c.score->total << '\n';
        for (const auto &x : c.reasons) std::cout << " reason=" << x << '\n';
        for (const auto &x : c.risks) std::cout << " risk=" << x << '\n';
    }
    for (const auto &e : r.excluded) { std::cout << " excluded=" << e.workerId << " reason-codes="; for (auto reason : e.reasons) std::cout << static_cast<int>(reason) << ','; std::cout << '\n'; }
    auto leastLoad = r.eligible;
    std::sort(leastLoad.begin(), leastLoad.end(), [](const auto &a, const auto &b) { return a.activeTasks != b.activeTasks ? a.activeTasks < b.activeTasks : a.workerId < b.workerId; });
    std::cout << " test-only least-load baseline="; for (const auto &c : leastLoad) std::cout << c.workerId << ','; std::cout << '\n';
}
}

int main() { try {
    test("real strategy is polymorphic", [] {
        std::unique_ptr<DispatchStrategy> s = std::make_unique<WeightedScoreStrategy>();
        const auto r = s->recommend(order(), {worker()}); consistent(r); near(r.eligible.at(0).score->total, 87);
    });
    test("real filter runs before history scoring and preserves exclusions", [] {
        auto w = worker(); w.record.skills.clear(); w.history.efficiency.reset(); w.history.rating.reset();
        const auto r = WeightedScoreStrategy{}.recommend(order(), {w}); check(r.eligible.empty()); sameExclusions(r, EligibilityFilter::filter(order(), {w}));
    });
    test("all required skill levels use floating division", [] {
        for (int n = 1; n <= 5; ++n) { auto w = worker(); w.record.skills[0].level = n; near(single(w).score->components.skill, n * 20); }
    });
    test("unrelated skill cannot increase required skill score", [] {
        auto w = worker(); w.record.skills[0].level = 1; w.record.skills.insert(w.record.skills.begin(), {9, 5, true}); near(single(w).score->components.skill, 20);
    });
    test("verified same building has area 100", [] { near(single(worker()).score->components.area, 100); });
    test("campus worker serving a building has area 70 not fabricated same-building", [] {
        auto w = worker(); w.record.serviceAreaId = 11; w.serviceArea = {{}, 11, std::nullopt, true}; const auto c = single(w);
        near(c.score->components.area, 70); check(has(c.reasons, "Verified campus coverage") && !has(c.reasons, "Verified building coverage"));
    });
    test("campus node and outdoor location keep the same campus coverage rule", [] {
        auto w = worker(); w.record.serviceAreaId = 11; w.serviceArea = {{}, 11, std::nullopt, true};
        auto o = order(); o.locationId = 11; o.location = {{}, 11, std::nullopt, true}; near(single(w, o).score->components.area, 70);
        o.locationId = 77; o.location = {{11}, 11, std::nullopt, true}; near(single(w, o).score->components.area, 70);
    });
    test("cross-building worker is never revived for 70 points", [] {
        auto w = worker(); w.record.serviceAreaId = 23; w.serviceArea = {{11}, 11, 23, true};
        const auto r = WeightedScoreStrategy{}.recommend(order(), {w}); check(r.eligible.empty()); check(r.excluded.at(0).reasons == std::vector<Reason>{Reason::OutsideServiceArea});
    });
    test("different campus never produces a 30-point candidate", [] {
        auto w = worker(); w.record.serviceAreaId = 12; w.serviceArea = {{}, 12, std::nullopt, true};
        const auto r = WeightedScoreStrategy{}.recommend(order(), {w}); check(r.eligible.empty()); check(r.excluded.at(0).reasons == std::vector<Reason>{Reason::OutsideServiceArea});
    });
    test("unverified location stays excluded", [] {
        auto o = order(); o.location.verified = false; const auto r = WeightedScoreStrategy{}.recommend(o, {worker()});
        check(r.eligible.empty() && r.excluded.at(0).reasons == std::vector<Reason>{Reason::LocationUnverified});
    });
    test("load fractions are rounded once to one decimal", [] {
        const std::array<double, 3> expected{100, 66.7, 33.3};
        for (int n = 0; n < 3; ++n) { auto w = worker(); w.record.activeTasks = n; near(single(w).score->components.load, expected.at(n)); }
    });
    test("full and overloaded workers stay excluded even with ideal history", [] {
        for (int n : {3, 4}) { auto w = worker(); w.record.activeTasks = n; efficiency(w, {1, 1, 1}); ratings(w, {5, 5, 5});
            const auto r = WeightedScoreStrategy{}.recommend(order(), {w}); check(r.eligible.empty() && r.excluded.at(0).reasons == std::vector<Reason>{Reason::AtCapacity}); }
    });
    test("efficiency zero one two verified samples use cold start", [] {
        for (int n = 0; n < 3; ++n) { auto w = worker(); efficiency(w, std::vector<double>(n, 1)); const auto c = single(w);
            near(c.score->components.efficiency, 60); check(c.efficiencySamples == static_cast<std::size_t>(n)); }
    });
    test("efficiency 100 100 50 aggregates to 83.3", [] { auto w = worker(); efficiency(w, {30, 60, 120}); near(single(w).score->components.efficiency, 83.3); });
    test("actual minutes zero and sub-minute clamp to one", [] { auto w = worker(); efficiency(w, {0, 0.1, 0.9}, 1); near(single(w).score->components.efficiency, 100); });
    test("efficiency twenty-sample boundary", [] { auto w = worker(); efficiency(w, std::vector<double>(20, 120)); const auto c = single(w); near(c.score->components.efficiency, 50); check(c.efficiencySamples == 20); });
    test("raw efficiency samples are not rounded individually", [] {
        auto w = worker(); efficiency(w, {100.0 / 33.34, 100.0 / 33.34, 100.0 / 33.44}, 1); near(single(w).score->components.efficiency, 33.4);
    });
    test("rating zero one two verified samples use cold start", [] {
        for (int n = 0; n < 3; ++n) { auto w = worker(); ratings(w, std::vector<std::int64_t>(n, 5)); const auto c = single(w);
            near(c.score->components.rating, 60); check(c.ratingSamples == static_cast<std::size_t>(n)); }
    });
    test("rating four five four aggregates to 86.7", [] { auto w = worker(); ratings(w, {4, 5, 4}); near(single(w).score->components.rating, 86.7); });
    test("rating twenty-sample boundary", [] { auto w = worker(); ratings(w, std::vector<std::int64_t>(20, 1)); const auto c = single(w); near(c.score->components.rating, 20); check(c.ratingSamples == 20); });
    test("efficiency and rating cold start independently", [] {
        auto w = worker(); efficiency(w, {30, 60, 120}); auto c = single(w); near(c.score->components.efficiency, 83.3); near(c.score->components.rating, 60); check(c.risks.size() == 1);
        w = worker(); ratings(w, {4, 5, 4}); c = single(w); near(c.score->components.efficiency, 60); near(c.score->components.rating, 86.7); check(c.risks.size() == 1);
    });
    test("unloaded eligible efficiency is not zero samples", [] { auto w = worker(); w.history.efficiency.reset(); invalid([&] { single(w); }); });
    test("unloaded eligible rating is not zero samples", [] { auto w = worker(); w.history.rating.reset(); invalid([&] { single(w); }); });
    test("one incomplete eligible history rejects entire recommendation", [] {
        auto a = worker(1), b = worker(2); b.history.rating.reset(); invalid([&] { WeightedScoreStrategy{}.recommend(order(), {a, b}); });
    });
    test("ordinary order gives no on-duty bonus", [] { auto w = worker(); w.record.emergencyDuty = true; near(single(w).score->components.duty, 0); });
    test("HIGH is not URGENT", [] { auto o = order(); o.approvedUrgency = "HIGH"; auto w = worker(); w.record.emergencyDuty = true; near(single(w, o).score->components.duty, 0); });
    test("URGENT on-duty worker gets 100", [] { auto o = order(); o.approvedUrgency = "URGENT"; auto w = worker(); w.record.emergencyDuty = true; near(single(w, o).score->components.duty, 100); });
    test("URGENT off-duty-flag worker gets zero bonus", [] { auto o = order(); o.approvedUrgency = "URGENT"; near(single(worker(), o).score->components.duty, 0); });
    test("urgent bonus cannot bypass any real hard exclusion", [] {
        auto o = order(); o.approvedUrgency = "URGENT"; o.categoryRequiresCertificate = true;
        auto base = worker(); base.record.emergencyDuty = true; base.record.skills[0].certified = true;
        std::vector<DispatchWorker> input; auto w = base; w.record.active = false; input.push_back(w);
        w = base; w.record.id = 2; w.accountRole = "sysadmin"; input.push_back(w);
        w = base; w.record.id = 3; w.record.availability = "BUSY"; input.push_back(w);
        w = base; w.record.id = 4; w.record.skills[0].certified = false; input.push_back(w);
        w = base; w.record.id = 5; w.record.skills.clear(); input.push_back(w);
        w = base; w.record.id = 6; w.record.activeTasks = 3; input.push_back(w);
        w = base; w.record.id = 7; w.record.serviceAreaId = 23; w.serviceArea = {{11}, 11, 23, true}; input.push_back(w);
        w = base; w.record.id = 8; w.accountStatus = "DISABLED"; input.push_back(w);
        const auto r = WeightedScoreStrategy{}.recommend(o, input); check(r.eligible.empty() && r.excluded.size() == 8); sameExclusions(r, EligibilityFilter::filter(o, input));
    });
    test("fixed six weights and independently recomputable total", [] {
        check(p0Weights == std::array<double, 6>{0.35, 0.20, 0.20, 0.10, 0.10, 0.05});
        auto w = worker(); w.record.skills[0].level = 4; w.record.activeTasks = 1; efficiency(w, {30, 60, 120}); ratings(w, {4, 5, 4});
        const auto r = WeightedScoreStrategy{}.recommend(order(), {w}); consistent(r); near(r.eligible.at(0).score->total, 78.3);
    });
    test("decimal half-up total boundary is not binary-rounded downward", [] {
        auto w = worker(); w.record.activeTasks = 1; const auto below = single(w); near(below.score->total, 80.3);
        efficiency(w, std::vector<double>(3, 6000.0 / 60.1)); const auto half = single(w);
        near(half.score->components.efficiency, 60.1); near(half.score->total, 80.4);
    });
    test("near scores inside one displayed bucket tie by ID not hidden precision", [] {
        auto a = worker(1), b = worker(2); efficiency(b, std::vector<double>(3, 6000.0 / 60.1));
        const auto r = WeightedScoreStrategy{}.recommend(order(), {b, a}); near(r.eligible.at(0).score->total, 87); near(r.eligible.at(1).score->total, 87);
        check(r.eligible.at(0).workerId == 1 && r.eligible.at(1).workerId == 2);
    });
    test("a higher rounded total wins before load or ID", [] {
        auto a = worker(1), b = worker(2); a.record.activeTasks = 1; b.record.activeTasks = 1; efficiency(b, std::vector<double>(3, 6000.0 / 60.1));
        const auto r = WeightedScoreStrategy{}.recommend(order(), {a, b}); check(r.eligible.at(0).workerId == 2); near(r.eligible.at(0).score->total, 80.4);
    });
    test("equal totals break by lower absolute activeTasks", [] {
        auto a = worker(20), b = worker(10); a.record.activeTasks = 1; b.record.activeTasks = 2; b.record.capacity = 6;
        const auto r = WeightedScoreStrategy{}.recommend(order(), {b, a}); near(r.eligible.at(0).score->total, 80.3); near(r.eligible.at(1).score->total, 80.3);
        check(r.eligible.at(0).workerId == 20 && r.eligible.at(1).workerId == 10);
    });
    test("equal totals and load break by numeric ID", [] {
        const auto r = WeightedScoreStrategy{}.recommend(order(), {worker(10), worker(2)}); check(r.eligible.at(0).workerId == 2 && r.eligible.at(1).workerId == 10);
    });
    test("every permutation yields identical scores reasons exclusions and continuous rank", [] {
        std::vector<DispatchWorker> input{worker(1), worker(2), worker(3), worker(4), worker(5), worker(6)};
        input[0].record.skills[0].level = 1; input[1].record.activeTasks = 1; input[2].record.activeTasks = 3; input[3].record.skills.clear();
        const auto expected = resultSnapshot(WeightedScoreStrategy{}.recommend(order(), input));
        do { check(resultSnapshot(WeightedScoreStrategy{}.recommend(order(), input)) == expected); }
        while (std::next_permutation(input.begin(), input.end(), [](const auto &a, const auto &b) { return a.record.id < b.record.id; }));
        for (int n = 0; n < 5; ++n) check(resultSnapshot(WeightedScoreStrategy{}.recommend(order(), input)) == expected);
        consistent(WeightedScoreStrategy{}.recommend(order(), input));
    });
    test("one or two eligible candidates work without padding", [] {
        check(WeightedScoreStrategy{}.recommend(order(), {worker(1)}).eligible.size() == 1);
        const auto r = WeightedScoreStrategy{}.recommend(order(), {worker(1), worker(2)}); check(r.eligible.size() == 2); consistent(r);
    });
    test("fourth and later candidates remain available for API split later", [] {
        const auto r = WeightedScoreStrategy{}.recommend(order(), {worker(5), worker(4), worker(3), worker(2), worker(1)});
        check(r.eligible.size() == 5 && r.eligible.at(3).rank == 4 && r.eligible.at(4).rank == 5); consistent(r);
    });
    test("no eligible candidates preserves every independent failure", [] {
        auto w = worker(); w.accountStatus = "DISABLED"; w.record.active = false; w.record.skills.clear(); w.record.activeTasks = 4;
        const auto r = WeightedScoreStrategy{}.recommend(order(), {w}); check(r.eligible.empty()); sameExclusions(r, EligibilityFilter::filter(order(), {w}));
        check(r.excluded.at(0).reasons == std::vector<Reason>{Reason::AccountInactive, Reason::WorkerInactive, Reason::MissingSkill, Reason::AtCapacity});
    });
    test("empty personnel returns no synthetic score", [] { const auto r = WeightedScoreStrategy{}.recommend(order(), {}); check(r.eligible.empty() && r.excluded.empty()); });
    test("complete input snapshots stay unchanged on success and failure", [] {
        auto o = order(); std::vector<DispatchWorker> input{worker(1), worker(2)}; input[0].record.workerNo = "Fictional"; input[0].record.displayName = "Private marker";
        efficiency(input[0], {30, 60, 120}); ratings(input[0], {4, 5, 4}); const auto before = inputSnapshot(o, input);
        WeightedScoreStrategy{}.recommend(o, input); check(inputSnapshot(o, input) == before);
        input[1].history.rating.reset(); const auto failedBefore = inputSnapshot(o, input); invalid([&] { WeightedScoreStrategy{}.recommend(o, input); }); check(inputSnapshot(o, input) == failedBefore);
    });
    test("64-bit order and personnel IDs preserve type and numeric order", [] {
        auto o = order(); o.id = std::numeric_limits<BusinessId>::max(); auto w = worker(o.id);
        const auto r = WeightedScoreStrategy{}.recommend(o, {w, worker(4000000002LL)}); check(o.id == std::numeric_limits<BusinessId>::max());
        check(r.eligible.at(1).workerId == w.record.id && r.eligible.at(0).workerId == 4000000002LL);
    });
    test("cold-start explanations match exact counts without fictional praise", [] {
        auto w = worker(); efficiency(w, {1}); ratings(w, {5, 5}); const auto c = single(w);
        check(c.efficiencySamples == 1 && c.ratingSamples == 2 && c.risks.size() == 2);
        check(has(c.risks, "Efficiency samples below 3; cold-start score 60") && has(c.risks, "Rating samples below 3; cold-start score 60"));
        check(has(c.reasons, "Efficiency cold start from 1 samples") && has(c.reasons, "Rating cold start from 2 samples"));
    });
    test("non-cold-start histories do not claim missing samples", [] {
        auto w = worker(); efficiency(w, {30, 60, 120}); ratings(w, {4, 5, 4}); const auto c = single(w);
        check(c.risks.empty() && c.efficiencySamples == 3 && c.ratingSamples == 3);
        check(has(c.reasons, "Efficiency mean from 3 valid samples") && has(c.reasons, "Rating mean from 3 valid samples"));
    });
    test("bonus explanation appears only for real bonus", [] {
        auto w = worker(); w.record.emergencyDuty = true; check(!has(single(w).reasons, "Urgent on-duty bonus applied"));
        auto o = order(); o.approvedUrgency = "URGENT"; check(has(single(w, o).reasons, "Urgent on-duty bonus applied"));
    });
    test("recommendation reasons include only required level area and true load", [] {
        auto w = worker(); w.record.skills[0].level = 2; w.record.activeTasks = 2; w.record.displayName = "SECRET_DISPLAY"; w.record.workerNo = "SECRET_NUMBER";
        const auto c = single(w); check(has(c.reasons, "Required skill level 2/5") && has(c.reasons, "Current load 2/3"));
        const auto text = resultSnapshot(WeightedScoreStrategy{}.recommend(order(), {w})); check(text.find("SECRET_") == std::string::npos);
    });
    test("NaN infinity negative duration invalid rating and duplicate samples fail", [] {
        for (double n : {std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::infinity(), -1.0}) { auto w = worker(); efficiency(w, {n}); invalid([&] { single(w); }); }
        auto w = worker(); ratings(w, {0}); invalid([&] { single(w); }); w = worker(); efficiency(w, {30, 60, 120});
        w.history.efficiency->at(1).assignmentId = w.history.efficiency->at(0).assignmentId; invalid([&] { single(w); });
    });
    test("more than twenty samples is rejected not silently truncated", [] {
        auto w = worker(); efficiency(w, std::vector<double>(21, 60)); invalid([&] { single(w); }); w = worker(); ratings(w, std::vector<std::int64_t>(21, 4)); invalid([&] { single(w); });
    });
    test("finite extreme durations and target IDs do not overflow score", [] {
        auto w = worker(); efficiency(w, std::vector<double>(3, std::numeric_limits<double>::max()), 1); near(single(w).score->components.efficiency, 0);
        efficiency(w, {1, 1, 1}, std::numeric_limits<std::int64_t>::max()); near(single(w).score->components.efficiency, 100);
    });
    test("maximum score and all returned components remain bounded", [] {
        auto o = order(); o.approvedUrgency = "URGENT"; auto w = worker(); w.record.emergencyDuty = true; efficiency(w, {1, 1, 1}); ratings(w, {5, 5, 5});
        const auto r = WeightedScoreStrategy{}.recommend(o, {w}); near(r.eligible.at(0).score->total, 100); consistent(r);
    });
    test("shared validation rejects duplicate workers and wrong order state", [] {
        invalid([] { WeightedScoreStrategy{}.recommend(order(), {worker(), worker()}); }); auto o = order(); o.status = "DRAFT"; invalid([&] { WeightedScoreStrategy{}.recommend(o, {}); });
    });
    test("experiment ordinary multi-candidate", [] {
        auto a = worker(1), b = worker(2), c = worker(3); a.record.activeTasks = 1; b.record.skills[0].level = 4;
        c.record.serviceAreaId = 11; c.serviceArea = {{}, 11, std::nullopt, true}; const std::vector<DispatchWorker> input{a, b, c};
        const auto r = WeightedScoreStrategy{}.recommend(order(), input); check(r.eligible.at(0).workerId == 3 && r.eligible.at(1).workerId == 1 && r.eligible.at(2).workerId == 2);
        near(r.eligible.at(0).score->total, 81); near(r.eligible.at(1).score->total, 80.3); near(r.eligible.at(2).score->total, 80); experiment("ordinary", order(), input, r);
    });
    test("experiment urgent on-duty", [] {
        auto o = order(); o.approvedUrgency = "URGENT"; auto a = worker(1), b = worker(2); b.record.emergencyDuty = true;
        const auto r = WeightedScoreStrategy{}.recommend(o, {a, b}); check(r.eligible.at(0).workerId == 2); near(r.eligible.at(0).score->total, 92); near(r.eligible.at(1).score->total, 87); experiment("urgent", o, {a, b}, r);
    });
    test("experiment skill mismatch", [] {
        auto a = worker(1), b = worker(2); a.record.skills = {{9, 5, true}}; const auto r = WeightedScoreStrategy{}.recommend(order(), {a, b});
        check(r.eligible.at(0).workerId == 2 && r.excluded.at(0).reasons == std::vector<Reason>{Reason::MissingSkill}); experiment("skill-mismatch", order(), {a, b}, r);
    });
    test("experiment full capacity", [] {
        auto a = worker(1), b = worker(2); a.record.activeTasks = 3; const auto r = WeightedScoreStrategy{}.recommend(order(), {a, b});
        check(r.eligible.at(0).workerId == 2 && r.excluded.at(0).reasons == std::vector<Reason>{Reason::AtCapacity}); experiment("at-capacity", order(), {a, b}, r);
    });
    test("experiment tied scores break by load then ID", [] {
        auto a = worker(20), b = worker(10), c = worker(5); a.record.activeTasks = 1; b.record.activeTasks = 2; b.record.capacity = 6; c.record.activeTasks = 1;
        const auto r = WeightedScoreStrategy{}.recommend(order(), {a, b, c}); check(r.eligible.at(0).workerId == 5 && r.eligible.at(1).workerId == 20 && r.eligible.at(2).workerId == 10);
        for (const auto &candidate : r.eligible) near(candidate.score->total, 80.3); experiment("ties", order(), {a, b, c}, r);
    });
    test("experiment all excluded", [] {
        auto a = worker(1), b = worker(2), c = worker(3); a.record.skills.clear(); b.record.activeTasks = 3; c.record.active = false;
        const auto r = WeightedScoreStrategy{}.recommend(order(), {a, b, c}); check(r.eligible.empty() && r.excluded.size() == 3); sameExclusions(r, EligibilityFilter::filter(order(), {a, b, c})); experiment("all-excluded", order(), {a, b, c}, r);
    });
    std::cout << "PASS dispatch_weighted_score: " << cases << " cases, " << assertions << " assertions; real filter + scoring; synthetic snapshots only\n";
    return 0;
} catch (const std::exception &error) { std::cerr << error.what() << '\n'; return 1; } }
