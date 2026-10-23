#pragma once
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace campusfix {
enum class OrderView { Reported,Assigned,Managed };
struct OrderQuery {
    OrderView view=OrderView::Reported;
    std::int64_t page=1,pageSize=20;
    std::optional<std::string> status;
    std::optional<std::int64_t> categoryId;
};
struct OrderReference {std::int64_t id=0;std::string code,name;};
// Explicit read models; credentials, storage paths and raw event JSON have no fields here.
struct OrderSummary {
    std::int64_t id=0,version=0;
    std::optional<std::string> orderNo,approvedUrgency,submittedAt;
    std::string title,urgency,status,createdAt,updatedAt;
    OrderReference category,location;
};
struct OrderPage {std::vector<OrderSummary> items;std::int64_t page=1,pageSize=20,total=0;};
struct OrderContact {
    std::string description,name,phone;
    std::optional<std::string> expectedVisitAt;
};
struct OrderAssignment {std::int64_t workerId=0;std::string workerName;};
struct OrderDetail {
    OrderSummary summary;
    std::optional<OrderContact> contact;
    std::optional<OrderAssignment> currentAssignment;
};
void validateOrderQuery(const OrderQuery &query);
}
