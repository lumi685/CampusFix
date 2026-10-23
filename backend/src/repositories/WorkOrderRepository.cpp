#include "repositories/WorkOrderRepository.h"
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
    auto c=db_.prepare("SELECT 1 FROM fault_categories WHERE id=?1 AND active=1");c.bindInt(1,i.categoryId);
    if(!c.step()) throw WorkOrderError(422,"VALIDATION_ERROR","Invalid or inactive category","category_id");
    auto l=db_.prepare("SELECT 1 FROM locations WHERE id=?1 AND active=1");l.bindInt(1,i.locationId);
    if(!l.step()) throw WorkOrderError(422,"VALIDATION_ERROR","Invalid or inactive location","location_id");
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
void WorkOrderRepository::log(std::int64_t order,std::int64_t actor,const std::string &action,
    const std::optional<std::string> &from,const std::string &to,const std::string &utc,const std::string &requestId) {
    auto q=db_.prepare("INSERT INTO status_logs(order_id,actor_id,action,from_status,to_status,created_at,request_id) "
        "VALUES(?1,?2,?3,?4,?5,?6,?7)");
    q.bindInt(1,order);q.bindInt(2,actor);q.bindText(3,action);
    if(from) q.bindText(4,*from);else q.bindNull(4);
    q.bindText(5,to);q.bindText(6,utc);q.bindText(7,requestId);q.step();
}
}
