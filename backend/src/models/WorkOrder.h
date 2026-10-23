#pragma once
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string>

namespace campusfix {
struct WorkOrderInput {
    std::int64_t categoryId=0, locationId=0;
    std::string title,description,urgency,contactName,contactPhone;
    std::optional<std::string> expectedVisitAt;
};
struct WorkOrder {
    std::int64_t id=0,reporterId=0,version=1;
    std::optional<std::string> orderNo,submittedAt;
    std::string status="DRAFT";
    WorkOrderInput input;
};
class WorkOrderError:public std::runtime_error {
public:
    WorkOrderError(int http,std::string businessCode,std::string message,std::string fieldName={})
        :std::runtime_error(std::move(message)),status(http),code(std::move(businessCode)),field(std::move(fieldName)){}
    int status;
    std::string code,field;
    std::optional<std::int64_t> currentVersion;
    std::string currentStatus;
};
}
