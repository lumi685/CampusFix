#pragma once
#include <cstdint>
#include <optional>
#include <string>

namespace campusfix {
struct ReviewInput {
    std::string action;
    std::int64_t expectedVersion=0;
    std::optional<std::int64_t> categoryId;
    std::optional<std::string> approvedUrgency,reason,internalNote;
};
struct ReviewResult {
    std::int64_t id=0,version=0;
    std::string action,status;
};
}
