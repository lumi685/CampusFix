#pragma once
#include "models/WorkOrder.h"
#include "infrastructure/SqliteConnection.h"
#include "models/WorkOrderRead.h"

namespace campusfix {
class WorkOrderRepository {
public:
    explicit WorkOrderRepository(SqliteConnection &db):db_(db){}
    WorkOrder find(std::int64_t id);
    OrderPage list(std::int64_t authenticatedId,const std::string &managementCapability,const OrderQuery &query);
    OrderDetail detail(std::int64_t id,bool includeContact);
    void validateReferences(const WorkOrderInput &input);
    WorkOrder create(std::int64_t reporter,const WorkOrderInput &input,const std::string &utc);
    std::string allocateNumber(const std::string &date,const std::string &utc);
    void submit(const WorkOrder &order,const std::string &number,const std::string &utc);
    void edit(const WorkOrder &order,const WorkOrderInput &input,const std::string &utc);
    void log(std::int64_t order,std::int64_t actor,const std::string &action,
             const std::optional<std::string> &from,const std::string &to,
             const std::string &utc,const std::string &requestId);
private:
    SqliteConnection &db_;
};
}
