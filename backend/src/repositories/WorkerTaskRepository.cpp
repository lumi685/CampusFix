#include "repositories/WorkerTaskRepository.h"
#include "models/WorkOrder.h"
namespace campusfix {
CurrentTaskAssignment WorkerTaskRepository::current(std::int64_t order,std::int64_t user) {
    auto q=db_.prepare("SELECT a.id,a.version,a.accepted_at FROM assignments a JOIN workers w ON w.id=a.worker_id WHERE a.order_id=?1 AND w.user_id=?2 AND a.ended_at IS NULL");
    q.bindInt(1,order);q.bindInt(2,user);
    if(!q.step())throw WorkOrderError(409,"ORDER_STATUS_CONFLICT","Current assignment changed");
    CurrentTaskAssignment result{q.columnInt(0),q.columnInt(1),{}};if(!q.columnIsNull(2))result.acceptedAt=q.columnText(2);return result;
}
bool WorkerTaskRepository::repeatedRefusal(std::int64_t order,std::int64_t user,std::int64_t version) {
    // Only recognize the exact immediately preceding own refusal. Historical
    // transfer/other ownership does not confer any operation permission.
    auto q=db_.prepare("SELECT 1 FROM work_orders o JOIN assignments a ON a.order_id=o.id JOIN workers w ON w.id=a.worker_id WHERE o.id=?1 AND w.user_id=?2 AND o.status='PENDING_DISPATCH' AND o.version>1 AND o.version-1=?3 AND a.ended_at IS NOT NULL AND a.end_reason='REFUSED' AND NOT EXISTS(SELECT 1 FROM assignments n WHERE n.order_id=o.id AND (n.ended_at IS NULL OR n.id>a.id))");
    q.bindInt(1,order);q.bindInt(2,user);q.bindInt(3,version);return q.step();
}
void WorkerTaskRepository::change(const CurrentTaskAssignment &a,const std::string &utc,bool refuse) {
    auto q=db_.prepare(refuse?"UPDATE assignments SET ended_at=?1,end_reason='REFUSED',version=version+1 WHERE id=?2 AND version=?3 AND ended_at IS NULL AND accepted_at IS NULL":
        "UPDATE assignments SET accepted_at=?1,version=version+1 WHERE id=?2 AND version=?3 AND ended_at IS NULL AND accepted_at IS NULL");
    q.bindText(1,utc);q.bindInt(2,a.id);q.bindInt(3,a.version);q.step();
    if(sqlite3_changes(db_.nativeHandle())!=1)throw WorkOrderError(409,"ORDER_STATUS_CONFLICT","Assignment changed");
}
void WorkerTaskRepository::supersede(std::int64_t order,std::int64_t assignment,const std::string &utc) {
    auto q=db_.prepare("UPDATE transfer_requests SET status='SUPERSEDED',reviewed_by=NULL,review_reason='Assignment refused; pending transfer superseded',resolved_at=?1 WHERE order_id=?2 AND assignment_id=?3 AND status='PENDING'");
    q.bindText(1,utc);q.bindInt(2,order);q.bindInt(3,assignment);q.step();
}
void WorkerTaskRepository::advance(std::int64_t order,std::int64_t version,const std::string &target,const std::string &utc) {
    auto q=db_.prepare("UPDATE work_orders SET status=?1,version=version+1,updated_at=?2 WHERE id=?3 AND version=?4 AND status='PENDING_ACCEPT'");
    q.bindText(1,target);q.bindText(2,utc);q.bindInt(3,order);q.bindInt(4,version);q.step();
    if(sqlite3_changes(db_.nativeHandle())!=1)throw WorkOrderError(409,"ORDER_VERSION_CONFLICT","Order changed");
}
bool WorkerTaskRepository::owner(std::int64_t assignment,std::int64_t user) {
    auto q=db_.prepare("SELECT 1 FROM assignments a JOIN workers w ON w.id=a.worker_id WHERE a.id=?1 AND w.user_id=?2");q.bindInt(1,assignment);q.bindInt(2,user);return q.step();
}
}
