#include "repositories/DispatchRepository.h"
#include "domain/WorkOrderValidation.h"
#include "models/WorkOrder.h"

#include <algorithm>
#include <map>
#include <set>
#include <tuple>
#include <utility>

namespace campusfix {
namespace {
using Id = dispatch::BusinessId;
[[noreturn]] void invalidData() {
    throw WorkOrderError(500, "INTERNAL_ERROR", "Required dispatch data is unavailable or inconsistent");
}
struct Location {
    std::optional<Id> parent;
    std::string type;
    bool active = false;
};
using Locations = std::map<Id, Location>;

dispatch::LocationAncestry ancestry(Id id, const Locations &locations) {
    dispatch::LocationAncestry result;
    std::set<Id> visited;
    Id current = id;
    while (true) {
        const auto found = locations.find(current);
        if (found == locations.end() || !found->second.active || !visited.insert(current).second)
            return {}; // Nothing from an unverified partial path may confer scope.
        const auto &node = found->second;
        if (current != id) result.ancestors.push_back(current);
        if (node.type == "CAMPUS") {
            if (node.parent || result.campusId) return {};
            result.campusId = current;
            result.verified = true;
            return result;
        }
        if (!node.parent) return {};
        const auto parent = locations.find(*node.parent);
        if (parent == locations.end()) return {};
        const auto &parentType = parent->second.type;
        if (node.type == "BUILDING") {
            if (parentType != "CAMPUS" || result.buildingId) return {};
            result.buildingId = current;
        } else if (node.type == "FLOOR") {
            if (parentType != "BUILDING") return {};
        } else if (node.type == "ROOM") {
            if (parentType != "FLOOR") return {};
        } else if (node.type == "AREA") {
            if (parentType != "CAMPUS" && parentType != "BUILDING" && parentType != "FLOOR") return {};
        } else return {};
        current = *node.parent;
    }
}

// Exact UTC seconds/nanoseconds, independent of Windows time_t, local time and
// DST. validUtcDateTime checks the Gregorian calendar and at most nine digits.
struct Utc {
    std::int64_t seconds = 0, nanos = 0;
    bool operator<(const Utc &other) const {
        return std::tie(seconds, nanos) < std::tie(other.seconds, other.nanos);
    }
};
Utc timestamp(const std::string &value) {
    if (!validUtcDateTime(value)) invalidData();
    auto year = static_cast<std::int64_t>(std::stoi(value.substr(0, 4)));
    const auto month = static_cast<std::int64_t>(std::stoi(value.substr(5, 2)));
    const auto day = static_cast<std::int64_t>(std::stoi(value.substr(8, 2)));
    year -= month <= 2 ? 1 : 0;
    const auto era = (year >= 0 ? year : year - 399) / 400;
    const auto yoe = year - era * 400;
    const auto mp = month + (month > 2 ? -3 : 9);
    const auto doy = (153 * mp + 2) / 5 + day - 1;
    const auto doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    const auto days = era * 146097 + doe - 719468;
    Utc result;
    result.seconds = days * 86400 + std::stoll(value.substr(11, 2)) * 3600 +
                     std::stoll(value.substr(14, 2)) * 60 + std::stoll(value.substr(17, 2));
    if (value.size() > 20) {
        std::size_t digits = 0;
        for (std::size_t i = 20; i + 1 < value.size(); ++i, ++digits)
            result.nanos = result.nanos * 10 + (value[i] - '0');
        while (digits++ < 9) result.nanos *= 10;
    }
    return result;
}
double minutes(const Utc &start, const Utc &finish) {
    if (finish < start) invalidData();
    return (static_cast<double>(finish.seconds - start.seconds) +
            static_cast<double>(finish.nanos - start.nanos) / 1000000000.0) / 60.0;
}

template<class Sample> struct Historical {
    Utc time;
    Id id;
    Sample sample;
};
template<class Sample> void retainLatest(std::vector<Historical<Sample>> &rows, Historical<Sample> row) {
    const auto before = [](const auto &left, const auto &right) {
        if (right.time < left.time) return true;
        if (left.time < right.time) return false;
        return left.id > right.id;
    };
    const auto position = std::lower_bound(rows.begin(), rows.end(), row, before);
    rows.insert(position, std::move(row));
    if (rows.size() > 20) rows.pop_back();
}
std::string scopePredicate(std::size_t count) {
    std::string result = "w.service_area_id IN (";
    for (std::size_t i = 0; i < count; ++i) {
        if (i) result += ',';
        result += '?' + std::to_string(i + 1);
    }
    return result + ')';
}
void bindScope(SqliteStatement &query, const std::vector<Id> &scope) {
    for (std::size_t i = 0; i < scope.size(); ++i)
        query.bindInt(static_cast<int>(i + 1), scope[i]);
}
// A completed assignment must be the final assignment of a completed/closed
// order, not a previous transfer/refusal. Assignment ids are the durable insert
// sequence used to break ties; any later assignment disqualifies the old one.
const std::string completed =
    "a.end_reason='COMPLETED' AND a.ended_at IS NOT NULL "
    "AND o.status IN ('COMPLETED','CLOSED') "
    "AND NOT EXISTS(SELECT 1 FROM assignments newer WHERE newer.order_id=a.order_id AND newer.id>a.id)";
}

DispatchData DispatchRepository::load(std::int64_t orderId) {
    DispatchData result;
    {
        auto q = db_.prepare("SELECT o.id,o.version,o.status,o.category_id,o.location_id,o.approved_urgency,"
            "c.required_skill_id,c.requires_certificate,c.active,s.code,s.name,s.requires_certificate,s.active,"
            "c.target_completion_minutes FROM work_orders o LEFT JOIN fault_categories c ON c.id=o.category_id "
            "LEFT JOIN skills s ON s.id=c.required_skill_id WHERE o.id=?1");
        q.bindInt(1, orderId);
        if (!q.step()) throw WorkOrderError(404, "RESOURCE_NOT_FOUND", "Work order not found");
        if (q.columnIsNull(5) || q.columnIsNull(6) || q.columnIsNull(9) ||
            q.columnInt(8) != 1 || q.columnInt(12) != 1 || q.columnInt(13) <= 0) invalidData();
        auto &o = result.order;
        o.id = q.columnInt(0); o.version = q.columnInt(1); o.status = q.columnText(2);
        o.categoryId = q.columnInt(3); o.locationId = q.columnInt(4); o.approvedUrgency = q.columnText(5);
        o.requiredSkill = {q.columnInt(6), q.columnText(9), q.columnText(10), q.columnInt(11) != 0};
        o.categoryRequiresCertificate = q.columnInt(7) != 0;
        if (o.id <= 0 || o.version <= 0 || o.categoryId <= 0 || o.locationId <= 0 || o.requiredSkill.id <= 0 ||
            (o.approvedUrgency != "LOW" && o.approvedUrgency != "NORMAL" &&
             o.approvedUrgency != "HIGH" && o.approvedUrgency != "URGENT")) invalidData();
    }
    Locations locations;
    {
        auto q = db_.prepare("SELECT id,parent_id,location_type,active FROM locations ORDER BY id");
        while (q.step()) locations.emplace(q.columnInt(0), Location{
            q.columnIsNull(1) ? std::nullopt : std::optional<Id>{q.columnInt(1)}, q.columnText(2), q.columnInt(3) == 1});
    }
    result.order.location = ancestry(result.order.locationId, locations);
    if (!result.order.location.verified) invalidData();
    auto scope = result.order.location.ancestors;
    scope.push_back(result.order.locationId);
    const auto predicate = scopePredicate(scope.size());
    std::map<Id, std::size_t> index;
    {
        auto q = db_.prepare("SELECT w.id,w.user_id,w.worker_no,u.display_name,w.service_area_id,w.availability,"
            "w.max_concurrent_tasks,COALESCE(load.n,0),w.updated_at,w.active,w.emergency_duty,u.status,u.role "
            "FROM workers w LEFT JOIN users u ON u.id=w.user_id LEFT JOIN "
            "(SELECT worker_id,count(*) n FROM assignments WHERE ended_at IS NULL GROUP BY worker_id) load "
            "ON load.worker_id=w.id WHERE " + predicate + " ORDER BY w.id");
        bindScope(q, scope);
        while (q.step()) {
            if (q.columnIsNull(11) || q.columnIsNull(12)) invalidData();
            dispatch::DispatchWorker worker;
            auto &r = worker.record;
            r.id = q.columnInt(0); r.userId = q.columnInt(1); r.workerNo = q.columnText(2);
            r.displayName = q.columnText(3); r.serviceAreaId = q.columnInt(4); r.availability = q.columnText(5);
            r.capacity = q.columnInt(6); r.activeTasks = q.columnInt(7); r.updatedAt = q.columnText(8);
            r.active = q.columnInt(9) != 0; r.emergencyDuty = q.columnInt(10) != 0;
            worker.accountStatus = q.columnText(11); worker.accountRole = q.columnText(12);
            worker.serviceArea = ancestry(r.serviceAreaId, locations);
            index.emplace(r.id, result.workers.size());
            result.workers.push_back(std::move(worker));
        }
    }
    {
        auto q = db_.prepare("SELECT ws.worker_id,ws.skill_id,ws.level,ws.certified FROM worker_skills ws "
            "JOIN skills s ON s.id=ws.skill_id JOIN workers w ON w.id=ws.worker_id "
            "WHERE ws.active=1 AND s.active=1 AND " + predicate + " ORDER BY ws.worker_id,ws.skill_id");
        bindScope(q, scope);
        while (q.step()) result.workers.at(index.at(q.columnInt(0))).record.skills.push_back(
            {q.columnInt(1), q.columnInt(2), q.columnInt(3) != 0});
        // Never mark a failed query as a successfully loaded empty skill set.
        for (auto &worker : result.workers) worker.skillsLoaded = true;
    }
    std::map<Id, std::vector<Historical<dispatch::EfficiencySample>>> efficiency;
    {
        auto q = db_.prepare("SELECT a.worker_id,a.id,c.target_completion_minutes,a.accepted_at,"
            "a.result_submitted_at,a.ended_at FROM assignments a JOIN workers w ON w.id=a.worker_id "
            "JOIN work_orders o ON o.id=a.order_id JOIN fault_categories c ON c.id=o.category_id WHERE " +
            predicate + " AND " + completed + " AND a.accepted_at IS NOT NULL AND a.result_submitted_at IS NOT NULL "
            "ORDER BY a.worker_id,a.ended_at DESC,a.id DESC");
        bindScope(q, scope);
        while (q.step()) {
            const auto accepted = timestamp(q.columnText(3)), submitted = timestamp(q.columnText(4));
            const auto ended = timestamp(q.columnText(5));
            if (ended < submitted || q.columnInt(2) <= 0) invalidData();
            dispatch::EfficiencySample sample{q.columnInt(1), q.columnInt(2), minutes(accepted, submitted)};
            retainLatest(efficiency[q.columnInt(0)], Historical<dispatch::EfficiencySample>{ended, sample.assignmentId, sample});
        }
    }
    std::map<Id, std::vector<Historical<dispatch::RatingSample>>> ratings;
    {
        auto q = db_.prepare("SELECT a.worker_id,e.id,e.score,e.created_at FROM assignments a "
            "JOIN workers w ON w.id=a.worker_id JOIN work_orders o ON o.id=a.order_id "
            "JOIN evaluations e ON e.order_id=o.id WHERE " + predicate + " AND " + completed +
            " ORDER BY a.worker_id,e.created_at DESC,e.id DESC");
        bindScope(q, scope);
        while (q.step()) {
            if (q.columnInt(2) < 1 || q.columnInt(2) > 5) invalidData();
            dispatch::RatingSample sample{q.columnInt(1), q.columnInt(2)};
            retainLatest(ratings[q.columnInt(0)], Historical<dispatch::RatingSample>{timestamp(q.columnText(3)), sample.evaluationId, sample});
        }
    }
    // No partially loaded DispatchData escapes if any preparation/step/parse fails.
    for (auto &worker : result.workers) {
        worker.history.efficiency.emplace(); worker.history.rating.emplace();
        for (const auto &sample : efficiency[worker.record.id]) worker.history.efficiency->push_back(sample.sample);
        for (const auto &sample : ratings[worker.record.id]) worker.history.rating->push_back(sample.sample);
    }
    return result;
}
}
