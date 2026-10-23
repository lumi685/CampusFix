#pragma once
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace campusfix {
struct StatusLogEntry {
    std::int64_t orderId=0;
    std::optional<std::int64_t> actorId,assignmentId;
    std::string action,toStatus,createdAt,requestId;
    std::optional<std::string> fromStatus,publicNote,internalNote,metadataJson;
};
struct StatusLogRecord {
    std::int64_t id=0;
    std::string action,toStatus,createdAt;
    std::optional<std::string> fromStatus,publicNote,internalNote,metadataJson;
    std::optional<std::int64_t> actorId;
    std::string actorName;
};
struct TimelineQuery {std::int64_t page=1,pageSize=20;};
struct StatusLogPage {std::vector<StatusLogRecord> items;std::int64_t total=0;};
using ManagementValue=std::variant<std::int64_t,std::string>;
// Only projected fields enter this public read model. No raw metadata member.
struct TimelineItem {
    std::int64_t id=0;
    std::string action,toStatus,createdAt;
    std::optional<std::string> fromStatus,publicNote,internalNote;
    std::optional<std::int64_t> actorId;
    std::string actorName;
    std::map<std::string,ManagementValue> managementDetail;
};
struct TimelinePage {std::vector<TimelineItem> items;std::int64_t page=1,pageSize=20,total=0;};
void validateTimelineQuery(const TimelineQuery &query);
}
