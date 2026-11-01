#pragma once
#include "models/WorkOrder.h"
#include <vector>
namespace campusfix {
struct TransferQuery {
    std::string view;
    std::int64_t page=1,pageSize=20;
    std::optional<std::int64_t> orderId;
};
struct TransferItem {
    std::int64_t id=0,orderId=0,assignmentId=0,orderVersion=0;
    std::string reason,status,createdAt,orderStatus;
    std::optional<std::string> orderNo,reviewReason,resolvedAt;
};
struct TransferPage {std::int64_t page=1,pageSize=20,total=0;std::vector<TransferItem> items;};
class TransferDuplicate:public WorkOrderError {
public:
    explicit TransferDuplicate(std::int64_t id):WorkOrderError(409,"DUPLICATE_OPERATION","Pending transfer already exists"),requestId(id){}
    std::int64_t requestId;
};
}
