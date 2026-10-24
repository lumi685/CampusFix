#pragma once
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace campusfix {
struct WorkerSkill {std::int64_t skillId=0,level=0;bool certified=false;};
struct WorkerInput {
    std::int64_t serviceAreaId=0,capacity=0;
    std::string availability;
    bool emergencyDuty=false,active=false;
    std::vector<WorkerSkill> skills;
};
struct WorkerRecord {
    std::int64_t id=0,userId=0,serviceAreaId=0,capacity=0,activeTasks=0;
    std::string workerNo,displayName,availability,updatedAt;
    std::vector<WorkerSkill> skills;
};
struct WorkerQuery {
    std::int64_t page=1,pageSize=20;
    std::optional<std::string> availability;
    std::optional<std::int64_t> skillId;
    bool mine=false;
};
struct WorkerPage {std::vector<WorkerRecord> items;std::int64_t page=1,pageSize=20,total=0;};
struct WorkerLoad {
    std::int64_t workerId=0,activeTasks=0,capacity=0,remaining=0;
    std::string calculatedAt;
    bool overloaded=false; // Diagnostic only, not an extra API response field.
};
}
