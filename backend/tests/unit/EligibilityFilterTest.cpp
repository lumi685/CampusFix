#include "dispatch/EligibilityFilter.h"
#include <algorithm>
#include <iostream>
#include <limits>
#include <set>
#include <sstream>
#include <stdexcept>
#include <utility>

using namespace campusfix::dispatch;
namespace {
using Reason = ExclusionReason;
int assertions = 0, cases = 0;
void check(bool value) {
    ++assertions;
    if (!value) throw std::runtime_error("Eligibility assertion " + std::to_string(assertions) + " failed");
}
template<class F> void test(const char *name, F action) {
    action(); ++cases; std::cout << "PASS " << name << '\n';
}
template<class F> void invalid(F action) {
    try { action(); } catch (const std::invalid_argument &) { check(true); return; }
    throw std::runtime_error("Expected invalid_argument");
}
DispatchWorkOrder order() {
    DispatchWorkOrder o;
    o.id = 9000000001LL; o.version = 2; o.status = "PENDING_DISPATCH";
    o.categoryId = 3; o.requiredSkill = {8, "FICTIONAL", "Fictional skill", false};
    o.locationId = 44; o.location = {{33, 22, 11}, 11, 22, true};
    o.approvedUrgency = "NORMAL"; return o;
}
DispatchWorker worker(BusinessId id = 4000000001LL) {
    DispatchWorker w;
    w.record.id = id; w.record.userId = id; w.record.serviceAreaId = 22;
    w.record.active = true; w.record.availability = "AVAILABLE";
    w.record.capacity = 3; w.record.activeTasks = 0;
    w.accountStatus = "ACTIVE"; w.accountRole = "worker";
    w.record.skills = {{8, 1, false}}; w.skillsLoaded = true;
    w.serviceArea = {{11}, 11, 22, true}; return w;
}
void eligible(const DispatchResult &r, const DispatchWorker &w) {
    check(r.excluded.empty() && r.eligible.size() == 1);
    const auto &c = r.eligible.at(0);
    check(c.workerId == w.record.id);
    check(c.activeTasks == w.record.activeTasks && c.capacity == w.record.capacity);
    check(!c.score && !c.rank && !c.efficiencySamples && !c.ratingSamples);
}
void excluded(const DispatchResult &r, BusinessId id, std::vector<Reason> expected) {
    check(r.eligible.empty() && r.excluded.size() == 1);
    const auto &e = r.excluded.at(0);
    check(e.workerId == id && e.reasons == expected);
    check(std::set<Reason>(e.reasons.begin(), e.reasons.end()).size() == e.reasons.size());
    check(std::find(e.reasons.begin(), e.reasons.end(), Reason::Unknown) == e.reasons.end());
}
void reject(const DispatchWorker &w, std::vector<Reason> reasons, const DispatchWorkOrder &o = order()) {
    excluded(EligibilityFilter::filter(o, {w}), w.record.id, std::move(reasons));
}
std::string resultSnapshot(const DispatchResult &r) {
    std::ostringstream s;
    for (const auto &c : r.eligible) {
        s << "E" << c.workerId << '/' << c.activeTasks << '/' << c.capacity << '/' << c.score.has_value()
          << '/' << c.rank.has_value() << '/' << c.efficiencySamples.has_value() << '/' << c.ratingSamples.has_value();
        for (const auto &x : c.reasons) s << '|' << x;
        for (const auto &x : c.risks) s << '|' << x;
    }
    for (const auto &e : r.excluded) { s << "X" << e.workerId; for (auto x : e.reasons) s << '/' << static_cast<int>(x); }
    return s.str();
}
void append(std::ostream &s, const LocationAncestry &p) {
    for (auto id : p.ancestors) s << ',' << id;
    s << '|' << p.campusId.value_or(0) << '|' << p.buildingId.value_or(0) << '|' << p.verified;
}
std::string inputSnapshot(const DispatchWorkOrder &o, const std::vector<DispatchWorker> &workers) {
    std::ostringstream s;
    s << o.id << '|' << o.version << '|' << o.status << '|' << o.categoryId << '|' << o.locationId << '|' << o.approvedUrgency
      << '|' << o.requiredSkill.id << '|' << o.requiredSkill.code << '|' << o.requiredSkill.name
      << '|' << o.requiredSkill.requiresCertificate << '|' << o.categoryRequiresCertificate;
    append(s, o.location);
    for (const auto &w : workers) {
        const auto &r = w.record;
        s << '|' << r.id << '|' << r.userId << '|' << r.serviceAreaId << '|' << r.capacity << '|' << r.activeTasks
          << '|' << r.workerNo << '|' << r.displayName << '|' << r.availability << '|' << r.updatedAt << '|' << r.active << '|' << r.emergencyDuty;
        for (const auto &x : r.skills) s << '|' << x.skillId << '/' << x.level << '/' << x.certified;
        s << '|' << w.accountStatus << '|' << w.accountRole << '|' << w.skillsLoaded; append(s, w.serviceArea);
        s << '|' << w.history.efficiency.has_value() << '|' << w.history.rating.has_value();
        if (w.history.efficiency) for (const auto &x : *w.history.efficiency) s << '|' << x.assignmentId << '/' << x.targetCompletionMinutes << '/' << x.actualMinutes;
        if (w.history.rating) for (const auto &x : *w.history.rating) s << '|' << x.evaluationId << '/' << x.stars;
    }
    return s.str();
}
std::vector<DispatchWorker> mixed() {
    std::vector<DispatchWorker> w;
    for (BusinessId id = 1; id <= 6; ++id) w.push_back(worker(id));
    w[0].record.activeTasks = 2; // Two eligible with different real loads.
    w[1].record.skills.clear();
    w[2].record.serviceAreaId = 23; w[2].serviceArea = {{11}, 11, 23, true};
    w[3].record.activeTasks = 3;
    w[4].accountStatus = "DISABLED"; w[4].record.active = false;
    w[4].record.availability = "BUSY"; w[4].record.skills.clear(); w[4].record.activeTasks = 4;
    return w;
}
}

int main() { try {
    test("all requirements satisfied", [] { auto w = worker(); eligible(EligibilityFilter::filter(order(), {w}), w); });
    test("disabled account is distinct from inactive profile", [] { auto w = worker(); w.accountStatus = "DISABLED"; reject(w, {Reason::AccountInactive}); });
    test("all non-worker principal roles are excluded accurately", [] {
        for (const char *role : {"reporter", "dispatcher", "sysadmin"}) { auto w = worker(); w.accountRole = role; reject(w, {Reason::AccountRoleMismatch}); }
    });
    test("inactive worker profile", [] { auto w = worker(); w.record.active = false; reject(w, {Reason::WorkerInactive}); });
    test("only AVAILABLE may accept", [] {
        for (const char *state : {"BUSY", "OFF_DUTY", "DISABLED"}) { auto w = worker(); w.record.availability = state; reject(w, {Reason::Unavailable}); }
    });
    test("loaded empty skills or unrelated skill cannot qualify", [] {
        auto w = worker(); w.record.skills.clear(); reject(w, {Reason::MissingSkill});
        w.record.skills = {{9, 5, true}}; reject(w, {Reason::MissingSkill});
    });
    test("category certificate requirement", [] { auto o = order(); o.categoryRequiresCertificate = true; reject(worker(), {Reason::MissingCertificate}, o); });
    test("skill certificate requirement", [] { auto o = order(); o.requiredSkill.requiresCertificate = true; reject(worker(), {Reason::MissingCertificate}, o); });
    test("another skill certificate cannot substitute", [] {
        auto o = order(); o.categoryRequiresCertificate = true; auto w = worker(); w.record.skills.push_back({9, 5, true}); reject(w, {Reason::MissingCertificate}, o);
    });
    test("certificate is not invented for categories that do not require it", [] { auto w = worker(); eligible(EligibilityFilter::filter(order(), {w}), w); });
    test("required certificate passes and dual requirement has one reason", [] {
        auto o = order(); o.categoryRequiresCertificate = true; o.requiredSkill.requiresCertificate = true;
        auto w = worker(); reject(w, {Reason::MissingCertificate}, o); w.record.skills[0].certified = true;
        eligible(EligibilityFilter::filter(o, {w}), w);
    });
    test("valid levels one through five have no extra threshold", [] {
        for (int level = 1; level <= 5; ++level) { auto w = worker(); w.record.skills[0].level = level; eligible(EligibilityFilter::filter(order(), {w}), w); }
    });
    test("campus scope covers different buildings within its campus", [] {
        auto w = worker(); w.record.serviceAreaId = 11; w.serviceArea = {{}, 11, std::nullopt, true};
        eligible(EligibilityFilter::filter(order(), {w}), w);
        auto o = order(); o.locationId = 55; o.location = {{23, 11}, 11, 23, true}; eligible(EligibilityFilter::filter(o, {w}), w);
    });
    test("building scope covers its building floors and rooms", [] {
        auto o = order(); const auto w = worker(); eligible(EligibilityFilter::filter(o, {w}), w);
        o.locationId = 34; o.location = {{22, 11}, 11, 22, true}; eligible(EligibilityFilter::filter(o, {w}), w);
        o.locationId = 45; o.location = {{34, 22, 11}, 11, 22, true}; eligible(EligibilityFilter::filter(o, {w}), w);
        o.locationId = 22; o.location = {{11}, 11, 22, true}; eligible(EligibilityFilter::filter(o, {w}), w);
    });
    test("building scope cannot expand to all buildings of the same campus", [] {
        auto w = worker(); w.record.serviceAreaId = 23; w.serviceArea = {{11}, 11, 23, true}; reject(w, {Reason::OutsideServiceArea});
    });
    test("another campus is outside service area", [] {
        auto w = worker(); w.record.serviceAreaId = 12; w.serviceArea = {{}, 12, std::nullopt, true}; reject(w, {Reason::OutsideServiceArea});
        w.record.serviceAreaId = 24; w.serviceArea = {{12}, 12, 24, true}; reject(w, {Reason::OutsideServiceArea});
    });
    test("missing order metadata fails closed without claiming outside area", [] {
        auto o = order(); o.location = {}; reject(worker(), {Reason::LocationUnverified}, o);
        o = order(); o.location.verified = false; reject(worker(), {Reason::LocationUnverified}, o);
        o = order(); o.location.campusId.reset(); reject(worker(), {Reason::LocationUnverified}, o);
        o = order(); o.location.buildingId.reset(); reject(worker(), {Reason::LocationUnverified}, o);
    });
    test("unverified or unsupported service scope cannot grant eligibility", [] {
        auto w = worker(); w.serviceArea = {}; reject(w, {Reason::LocationUnverified});
        w = worker(); w.serviceArea.verified = false; reject(w, {Reason::LocationUnverified});
        w = worker(); w.record.serviceAreaId = 33; w.serviceArea = {{22, 11}, 11, 22, true}; reject(w, {Reason::LocationUnverified});
    });
    test("positive IDs and equal nodes alone are insufficient", [] {
        auto w = worker(); w.record.serviceAreaId = 44; w.serviceArea = {}; reject(w, {Reason::LocationUnverified});
        w = worker(); w.serviceArea.campusId.reset(); reject(w, {Reason::LocationUnverified});
    });
    test("zero and partial load retain exact snapshot values", [] {
        for (int load : {0, 2}) { auto w = worker(); w.record.activeTasks = load; eligible(EligibilityFilter::filter(order(), {w}), w); }
    });
    test("load equal to capacity is excluded", [] { auto w = worker(); w.record.activeTasks = 3; reject(w, {Reason::AtCapacity}); });
    test("overload is excluded not truncated", [] { auto w = worker(); w.record.activeTasks = 4; reject(w, {Reason::AtCapacity}); });
    test("urgent emergency-duty worker cannot bypass any hard constraint", [] {
        auto o = order(); o.approvedUrgency = "URGENT"; o.categoryRequiresCertificate = true;
        auto base = worker(); base.record.emergencyDuty = true; base.record.skills[0].certified = true;
        auto w = base; w.accountStatus = "DISABLED"; reject(w, {Reason::AccountInactive}, o);
        w = base; w.accountRole = "sysadmin"; reject(w, {Reason::AccountRoleMismatch}, o);
        w = base; w.record.active = false; reject(w, {Reason::WorkerInactive}, o);
        w = base; w.record.availability = "OFF_DUTY"; reject(w, {Reason::Unavailable}, o);
        w = base; w.record.skills.clear(); reject(w, {Reason::MissingSkill}, o);
        w = base; w.record.skills[0].certified = false; reject(w, {Reason::MissingCertificate}, o);
        w = base; w.record.serviceAreaId = 23; w.serviceArea = {{11}, 11, 23, true}; reject(w, {Reason::OutsideServiceArea}, o);
        w = base; w.record.activeTasks = 3; reject(w, {Reason::AtCapacity}, o);
        w = base; w.serviceArea.verified = false; reject(w, {Reason::LocationUnverified}, o);
    });
    test("all independent reasons collected once in stable rule order", [] {
        auto o = order(); o.categoryRequiresCertificate = true; auto w = worker();
        w.accountStatus = "DISABLED"; w.accountRole = "reporter"; w.record.active = false;
        w.record.availability = "DISABLED"; w.record.activeTasks = 9;
        w.record.serviceAreaId = 23; w.serviceArea = {{11}, 11, 23, true};
        reject(w, {Reason::AccountInactive, Reason::AccountRoleMismatch, Reason::WorkerInactive, Reason::Unavailable,
                   Reason::MissingCertificate, Reason::OutsideServiceArea, Reason::AtCapacity}, o);
    });
    test("no fabricated certificate fact when matching skill does not exist", [] {
        auto o = order(); o.requiredSkill.requiresCertificate = true; auto w = worker(); w.record.skills.clear(); reject(w, {Reason::MissingSkill}, o);
    });
    test("mixed six workers partition completely with precise reasons and loads", [] {
        const auto input = mixed(); const auto r = EligibilityFilter::filter(order(), input);
        check(r.eligible.size() == 2 && r.excluded.size() == 4);
        check(r.eligible[0].workerId == 1 && r.eligible[1].workerId == 6);
        check(r.excluded[0].workerId == 2 && r.excluded[0].reasons == std::vector<Reason>{Reason::MissingSkill});
        check(r.excluded[1].workerId == 3 && r.excluded[1].reasons == std::vector<Reason>{Reason::OutsideServiceArea});
        check(r.excluded[2].workerId == 4 && r.excluded[2].reasons == std::vector<Reason>{Reason::AtCapacity});
        check(r.excluded[3].workerId == 5 && r.excluded[3].reasons == std::vector<Reason>{Reason::AccountInactive, Reason::WorkerInactive, Reason::Unavailable, Reason::MissingSkill, Reason::AtCapacity});
        std::set<BusinessId> ids;
        for (const auto &c : r.eligible) {
            check(ids.insert(c.workerId).second); const auto &w = input.at(static_cast<std::size_t>(c.workerId - 1));
            check(c.activeTasks == w.record.activeTasks && c.capacity == w.record.capacity && !c.score && !c.rank);
        }
        for (const auto &e : r.excluded) check(ids.insert(e.workerId).second);
        check(ids == std::set<BusinessId>({1, 2, 3, 4, 5, 6}));
        validateResult(r, input);
    });
    test("all 720 permutations and repeat calls produce identical results", [] {
        auto input = mixed(); const auto expected = resultSnapshot(EligibilityFilter::filter(order(), input));
        do { check(resultSnapshot(EligibilityFilter::filter(order(), input)) == expected); }
        while (std::next_permutation(input.begin(), input.end(), [](const auto &a, const auto &b) { return a.record.id < b.record.id; }));
        for (int i = 0; i < 3; ++i) check(resultSnapshot(EligibilityFilter::filter(order(), input)) == expected);
    });
    test("invalid order structures reject the call instead of returning recommendations", [] {
        for (const char *state : {"", "DRAFT", "PENDING_REVIEW", "PENDING_ACCEPT", "CLOSED"}) {
            auto o = order(); o.status = state; invalid([&] { EligibilityFilter::filter(o, {worker()}); });
        }
        auto o = order(); o.id = 0; invalid([&] { EligibilityFilter::filter(o, {worker()}); });
        o = order(); o.version = -1; invalid([&] { EligibilityFilter::filter(o, {worker()}); });
        o = order(); o.requiredSkill.id = 0; invalid([&] { EligibilityFilter::filter(o, {worker()}); });
        o = order(); o.approvedUrgency.clear(); invalid([&] { EligibilityFilter::filter(o, {worker()}); });
    });
    test("invalid worker structures are not disguised as ordinary exclusions", [] {
        for (int capacity : {0, -1, 101}) { auto w = worker(); w.record.capacity = capacity; invalid([&] { EligibilityFilter::filter(order(), {w}); }); }
        auto w = worker(); w.record.activeTasks = -1; invalid([&] { EligibilityFilter::filter(order(), {w}); });
        w = worker(); w.record.id = -1; invalid([&] { EligibilityFilter::filter(order(), {w}); });
        w = worker(); w.accountRole = "invented"; invalid([&] { EligibilityFilter::filter(order(), {w}); });
        w = worker(); w.record.skills[0].level = 6; invalid([&] { EligibilityFilter::filter(order(), {w}); });
        w = worker(); w.record.skills.push_back(w.record.skills[0]); invalid([&] { EligibilityFilter::filter(order(), {w}); });
    });
    test("empty workers have no synthetic candidates but still validate order", [] {
        const auto r = EligibilityFilter::filter(order(), {}); check(r.eligible.empty() && r.excluded.empty());
        auto o = order(); o.version = 0; invalid([&] { EligibilityFilter::filter(o, {}); });
    });
    test("duplicate worker IDs reject the whole input", [] { invalid([] { EligibilityFilter::filter(order(), {worker(), worker()}); }); });
    test("64-bit max ID is preserved without string or narrowing sort", [] {
        auto large = worker(std::numeric_limits<BusinessId>::max()); auto small = worker(4000000002LL);
        const auto r = EligibilityFilter::filter(order(), {large, small});
        check(r.eligible.at(0).workerId == small.record.id && r.eligible.at(1).workerId == large.record.id);
    });
    test("entire original snapshots remain unchanged including load markers", [] {
        auto o = order(); auto input = mixed(); input[0].record.workerNo = "FICTITIOUS"; input[0].record.updatedAt = "fixture-time";
        input[0].history.efficiency = std::vector<EfficiencySample>{{1, 60, 20.5}};
        input[0].history.rating = std::vector<RatingSample>{{1, 5}};
        const auto before = inputSnapshot(o, input); EligibilityFilter::filter(o, input); check(inputSnapshot(o, input) == before);
        input[1].skillsLoaded = false; const auto failedBefore = inputSnapshot(o, input);
        invalid([&] { EligibilityFilter::filter(o, input); }); check(inputSnapshot(o, input) == failedBefore);
    });
    test("unknown zero and populated histories cannot manufacture score or eligibility", [] {
        auto w = worker(); const auto expected = resultSnapshot(EligibilityFilter::filter(order(), {w}));
        w.history.efficiency.emplace(); w.history.rating.emplace(); check(resultSnapshot(EligibilityFilter::filter(order(), {w})) == expected);
        w.history.efficiency = std::vector<EfficiencySample>{{1, 60, 2}}; w.history.rating = std::vector<RatingSample>{{1, 5}};
        check(resultSnapshot(EligibilityFilter::filter(order(), {w})) == expected); w.record.activeTasks = 3; reject(w, {Reason::AtCapacity});
    });
    test("skills not loaded is an explicit input failure even if vector is nonempty", [] {
        auto w = worker(); w.skillsLoaded = false; invalid([&] { EligibilityFilter::filter(order(), {w}); });
        w.record.skills.clear(); invalid([&] { EligibilityFilter::filter(order(), {w}); });
    });
    test("new reason codes are recognized but Unknown remains invalid", [] {
        const auto w = worker(); DispatchResult r; r.excluded = {{w.record.id, {Reason::AccountRoleMismatch, Reason::LocationUnverified}}};
        validateResult(r, {w}); check(true); r.excluded[0].reasons = {Reason::Unknown}; invalid([&] { validateResult(r, {w}); });
    });
    test("contradictory shared building/campus metadata is not guessed outside", [] {
        auto w = worker(); w.serviceArea = {{12}, 12, 22, true}; reject(w, {Reason::LocationUnverified});
    });
    test("claimed verification cannot repair malformed chain ordering", [] {
        auto o = order(); o.location.ancestors = {11, 22, 33}; reject(worker(), {Reason::LocationUnverified}, o);
        auto w = worker(); w.serviceArea.ancestors = {11, 12}; reject(w, {Reason::LocationUnverified});
    });
    test("campus locations and campus-level outdoor areas remain representable", [] {
        auto w = worker(); w.record.serviceAreaId = 11; w.serviceArea = {{}, 11, std::nullopt, true};
        auto o = order(); o.locationId = 11; o.location = {{}, 11, std::nullopt, true}; eligible(EligibilityFilter::filter(o, {w}), w);
        o.locationId = 77; o.location = {{11}, 11, std::nullopt, true}; eligible(EligibilityFilter::filter(o, {w}), w);
    });
    test("no candidates still has complete explanations without mutation", [] {
        auto w = worker(); w.record.skills.clear(); const auto o = order(); const auto before = inputSnapshot(o, {w});
        reject(w, {Reason::MissingSkill}, o); check(inputSnapshot(o, {w}) == before && o.status == "PENDING_DISPATCH");
    });
    test("new completeness markers default closed without changing domain construction", [] {
        const DispatchWorker w; const LocationAncestry p; check(!w.skillsLoaded && !p.verified);
        auto compatible = worker(); compatible.skillsLoaded = false; compatible.serviceArea.verified = false;
        validateWorker(compatible); check(true); // Generic W6-D1 validation is deliberately unchanged.
    });
    std::cout << "PASS dispatch_eligibility: " << cases << " cases, " << assertions << " assertions; real pure filter, no scoring or I/O\n";
    return 0;
} catch (const std::exception &error) { std::cerr << error.what() << '\n'; return 1; } }
