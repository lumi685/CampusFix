#include "repositories/WorkOrderRepository.h"
#include "repositories/StatusLogRepository.h"
#include <iomanip>
#include <sstream>

namespace campusfix {
WorkOrder WorkOrderRepository::find(std::int64_t id) {
    auto q=db_.prepare("SELECT id,reporter_id,status,version,order_no,submitted_at,category_id,location_id,"
        "title,description,requested_urgency,contact_name,contact_phone,expected_visit_at FROM work_orders WHERE id=?1");
    q.bindInt(1,id);
    if(!q.step()) throw WorkOrderError(404,"RESOURCE_NOT_FOUND","Work order not found");
    WorkOrder o;o.id=q.columnInt(0);o.reporterId=q.columnInt(1);o.status=q.columnText(2);o.version=q.columnInt(3);
    if(!q.columnIsNull(4)) o.orderNo=q.columnText(4);
    if(!q.columnIsNull(5)) o.submittedAt=q.columnText(5);
    o.input={q.columnInt(6),q.columnInt(7),q.columnText(8),q.columnText(9),q.columnText(10),q.columnText(11),q.columnText(12),{}};
    if(!q.columnIsNull(13)) o.input.expectedVisitAt=q.columnText(13);
    return o;
}
void WorkOrderRepository::validateReferences(const WorkOrderInput &i) {
    // Match CatalogRepository's effective hierarchy: an active child under an inactive
    // ancestor is not an available option. Fixed server-owned table names only.
    const auto valid=[&](const char *table,std::int64_t id) {
        auto q=db_.prepare(std::string("WITH RECURSIVE ancestors(id,parent_id,active) AS (")+
            "SELECT id,parent_id,active FROM "+table+" WHERE id=?1 UNION SELECT p.id,p.parent_id,p.active FROM "+table+
            " p JOIN ancestors a ON p.id=a.parent_id) SELECT 1 WHERE EXISTS(SELECT 1 FROM ancestors WHERE id=?1) "
            "AND NOT EXISTS(SELECT 1 FROM ancestors WHERE active<>1) AND EXISTS(SELECT 1 FROM ancestors WHERE parent_id IS NULL)");
        q.bindInt(1,id);return q.step();
    };
    if(!valid("fault_categories",i.categoryId)) throw WorkOrderError(422,"VALIDATION_ERROR","Invalid or inactive category hierarchy","category_id");
    if(!valid("locations",i.locationId)) throw WorkOrderError(422,"VALIDATION_ERROR","Invalid or inactive location hierarchy","location_id");
}
WorkOrder WorkOrderRepository::create(std::int64_t reporter,const WorkOrderInput &i,const std::string &utc) {
    auto q=db_.prepare("INSERT INTO work_orders(reporter_id,category_id,location_id,title,description,"
        "requested_urgency,contact_name,contact_phone,expected_visit_at,status,version,created_at,updated_at) "
        "VALUES(?1,?2,?3,?4,?5,?6,?7,?8,?9,'DRAFT',1,?10,?10)");
    q.bindInt(1,reporter);q.bindInt(2,i.categoryId);q.bindInt(3,i.locationId);
    q.bindText(4,i.title);q.bindText(5,i.description);q.bindText(6,i.urgency);
    q.bindText(7,i.contactName);q.bindText(8,i.contactPhone);
    if(i.expectedVisitAt) q.bindText(9,*i.expectedVisitAt);else q.bindNull(9);
    q.bindText(10,utc);q.step();
    return find(sqlite3_last_insert_rowid(db_.nativeHandle()));
}
std::string WorkOrderRepository::allocateNumber(const std::string &date,const std::string &utc) {
    auto q=db_.prepare("INSERT INTO order_number_sequences(business_date,last_value,updated_at) VALUES(?1,1,?2) "
        "ON CONFLICT(business_date) DO UPDATE SET last_value=last_value+1,updated_at=excluded.updated_at "
        "WHERE last_value<9999 RETURNING last_value");
    q.bindText(1,date);q.bindText(2,utc);
    if(!q.step()) throw WorkOrderError(500,"INTERNAL_ERROR","Daily order number capacity exhausted");
    const auto value=q.columnInt(0);
    // Consume RETURNING before COMMIT; statements must not retain a write cursor.
    q.step();
    std::ostringstream number;number<<"WX"<<date<<std::setfill('0')<<std::setw(4)<<value;
    return number.str();
}
void WorkOrderRepository::submit(const WorkOrder &o,const std::string &number,const std::string &utc) {
    auto q=db_.prepare("UPDATE work_orders SET order_no=?1,status='PENDING_REVIEW',version=version+1,"
        "submitted_at=COALESCE(submitted_at,?2),updated_at=?2 WHERE id=?3 AND status=?4 AND version=?5");
    q.bindText(1,number);q.bindText(2,utc);q.bindInt(3,o.id);q.bindText(4,o.status);q.bindInt(5,o.version);q.step();
    if(sqlite3_changes(db_.nativeHandle())!=1) throw WorkOrderError(409,"ORDER_VERSION_CONFLICT","Work order changed");
}
void WorkOrderRepository::edit(const WorkOrder &o,const WorkOrderInput &i,const std::string &utc) {
    auto q=db_.prepare("UPDATE work_orders SET category_id=?1,location_id=?2,title=?3,description=?4,"
        "requested_urgency=?5,contact_name=?6,contact_phone=?7,expected_visit_at=?8,updated_at=?9,version=version+1 "
        "WHERE id=?10 AND version=?11 AND status=?12 AND status IN ('DRAFT','PENDING_SUPPLEMENT')");
    q.bindInt(1,i.categoryId);q.bindInt(2,i.locationId);q.bindText(3,i.title);q.bindText(4,i.description);
    q.bindText(5,i.urgency);q.bindText(6,i.contactName);q.bindText(7,i.contactPhone);
    if(i.expectedVisitAt) q.bindText(8,*i.expectedVisitAt);else q.bindNull(8);
    q.bindText(9,utc);q.bindInt(10,o.id);q.bindInt(11,o.version);q.bindText(12,o.status);q.step();
    if(sqlite3_changes(db_.nativeHandle())!=1) throw WorkOrderError(409,"ORDER_VERSION_CONFLICT","Work order changed");
}
void WorkOrderRepository::log(std::int64_t order,std::int64_t actor,const std::string &action,
    const std::optional<std::string> &from,const std::string &to,const std::string &utc,const std::string &requestId) {
    // Compatibility for existing fixture callers; a single append implementation owns SQL.
    StatusLogEntry e;e.orderId=order;e.actorId=actor;e.action=action;e.fromStatus=from;
    e.toStatus=to;e.createdAt=utc;e.requestId=requestId;StatusLogRepository(db_).append(e);
}
}
