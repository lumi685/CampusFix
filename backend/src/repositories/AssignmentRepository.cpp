#include "repositories/AssignmentRepository.h"
#include "models/WorkOrder.h"

namespace campusfix {
bool AssignmentRepository::hasActive(std::int64_t orderId) {
    auto q=db_.prepare("SELECT 1 FROM assignments WHERE order_id=?1 AND ended_at IS NULL");q.bindInt(1,orderId);return q.step();
}
std::int64_t AssignmentRepository::insert(std::int64_t order,std::int64_t worker,std::int64_t actor,
                                         const std::optional<std::string> &reason,const std::string &utc) {
    auto q=db_.prepare("INSERT INTO assignments(order_id,worker_id,assigned_by,assign_reason,assigned_at) VALUES(?1,?2,?3,?4,?5)");
    q.bindInt(1,order);q.bindInt(2,worker);q.bindInt(3,actor);
    if(reason)q.bindText(4,*reason);else q.bindNull(4);q.bindText(5,utc);q.step();
    return sqlite3_last_insert_rowid(db_.nativeHandle());
}
void AssignmentRepository::snapshot(std::int64_t order,std::int64_t worker,std::int64_t actor,std::int64_t assignment,
                                   std::int64_t version,const std::string &weights,const std::string &candidates,
                                   const std::string &excluded,const std::string &utc) {
    auto q=db_.prepare("INSERT INTO dispatch_records(order_id,requested_by,selected_worker_id,assignment_id,order_version,"
        "strategy,weights_json,candidates_json,excluded_summary_json,created_at) VALUES(?1,?2,?3,?4,?5,'WEIGHTED_SCORE_V1',?6,?7,?8,?9)");
    q.bindInt(1,order);q.bindInt(2,actor);q.bindInt(3,worker);q.bindInt(4,assignment);q.bindInt(5,version);
    q.bindText(6,weights);q.bindText(7,candidates);q.bindText(8,excluded);q.bindText(9,utc);q.step();
}
void AssignmentRepository::advance(std::int64_t order,std::int64_t version,const std::string &target,const std::string &utc) {
    auto q=db_.prepare("UPDATE work_orders SET status=?1,version=version+1,updated_at=?2 WHERE id=?3 AND status='PENDING_DISPATCH' AND version=?4");
    q.bindText(1,target);q.bindText(2,utc);q.bindInt(3,order);q.bindInt(4,version);q.step();
    if(sqlite3_changes(db_.nativeHandle())!=1)throw WorkOrderError(409,"ORDER_VERSION_CONFLICT","Order changed; refresh and retry");
}
}
