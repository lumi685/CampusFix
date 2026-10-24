#pragma once
#include "models/WorkOrder.h"
#include <chrono>
#include <string>

namespace campusfix {
void validateWorkOrderInput(const WorkOrderInput &input);
void validateOrderText(const std::string &value,std::size_t max,const char *field,bool allowBlank=false);
bool validUtcDateTime(const std::string &value);
struct OrderTime { std::string utc,businessDate; };
// Asia/Shanghai (UTC+08:00) for current campus business dates, not host local time.
OrderTime orderTime(std::chrono::system_clock::time_point point);
}
