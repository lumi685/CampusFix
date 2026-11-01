#include "repositories/TransferRepository.h"
namespace campusfix {
TransferAssignment TransferRepository::current(std::int64_t order) {
    auto q=db_.prepare("SELECT a.id,a.version,w.user_id,a.accepted_at FROM assignments a JOIN workers w ON w.id=a.worker_id WHERE a.order_id=?1 AND a.ended_at IS NULL");q.bindInt(1,order);
    if(!q.step())throw WorkOrderError(409,"ORDER_STATUS_CONFLICT","Current assignment changed");
    TransferAssignment a{q.columnInt(0),q.columnInt(1),q.columnInt(2),{}};if(!q.columnIsNull(3))a.acceptedAt=q.columnText(3);return a;
}
TransferRecord TransferRepository::find(std::int64_t order,std::int64_t request) {
    auto q=db_.prepare("SELECT id,assignment_id,status FROM transfer_requests WHERE id=?1 AND order_id=?2");q.bindInt(1,request);q.bindInt(2,order);
    if(!q.step())throw WorkOrderError(404,"RESOURCE_NOT_FOUND","Transfer unavailable");return {q.columnInt(0),q.columnInt(1),q.columnText(2)};
}
std::optional<std::int64_t> TransferRepository::pending(std::int64_t assignment) {
    auto q=db_.prepare("SELECT id FROM transfer_requests WHERE assignment_id=?1 AND status='PENDING'");q.bindInt(1,assignment);return q.step()?std::optional<std::int64_t>(q.columnInt(0)):std::nullopt;
}
std::int64_t TransferRepository::insert(std::int64_t order,std::int64_t assignment,std::int64_t user,const std::string &reason,std::int64_t version,const std::string &utc) {
    auto q=db_.prepare("INSERT INTO transfer_requests(order_id,assignment_id,requested_by,reason,order_version,created_at) VALUES(?1,?2,?3,?4,?5,?6)");
    q.bindInt(1,order);q.bindInt(2,assignment);q.bindInt(3,user);q.bindText(4,reason);q.bindInt(5,version);q.bindText(6,utc);q.step();return sqlite3_last_insert_rowid(db_.nativeHandle());
}
void TransferRepository::resolve(std::int64_t request,std::int64_t assignment,std::int64_t user,const std::string &decision,const std::string &reason,const std::string &utc) {
    auto q=db_.prepare("UPDATE transfer_requests SET status=?1,reviewed_by=?2,review_reason=?3,resolved_at=?4 WHERE id=?5 AND assignment_id=?6 AND status='PENDING'");
    q.bindText(1,decision=="APPROVE"?"APPROVED":"REJECTED");q.bindInt(2,user);q.bindText(3,reason);q.bindText(4,utc);q.bindInt(5,request);q.bindInt(6,assignment);q.step();
    if(sqlite3_changes(db_.nativeHandle())!=1)throw WorkOrderError(409,"DUPLICATE_OPERATION","Transfer already resolved");
}
void TransferRepository::end(const TransferAssignment &a,const std::string &utc) {
    auto q=db_.prepare("UPDATE assignments SET ended_at=?1,end_reason='TRANSFERRED',version=version+1 WHERE id=?2 AND version=?3 AND ended_at IS NULL");q.bindText(1,utc);q.bindInt(2,a.id);q.bindInt(3,a.version);q.step();
    if(sqlite3_changes(db_.nativeHandle())!=1)throw WorkOrderError(409,"ORDER_STATUS_CONFLICT","Assignment changed");
}
void TransferRepository::advance(std::int64_t order,std::int64_t version,const std::string &from,const std::string &to,const std::string &utc) {
    auto q=db_.prepare("UPDATE work_orders SET status=?1,version=version+1,updated_at=?2 WHERE id=?3 AND version=?4 AND status=?5");q.bindText(1,to);q.bindText(2,utc);q.bindInt(3,order);q.bindInt(4,version);q.bindText(5,from);q.step();
    if(sqlite3_changes(db_.nativeHandle())!=1)throw WorkOrderError(409,"ORDER_VERSION_CONFLICT","Order changed");
}
bool TransferRepository::arrived(std::int64_t assignment) {
    auto q=db_.prepare("SELECT 1 FROM status_logs WHERE assignment_id=?1 AND action='WORKER_ARRIVED'");q.bindInt(1,assignment);return q.step();
}
TransferPage TransferRepository::list(std::int64_t user,const std::string &cap,const TransferQuery &input) {
    // UNION terminates on overlapping/cyclic location graphs; EXISTS/IN never
    // multiplies rows. Every dynamic value is a bound parameter.
    const std::string cte="WITH RECURSIVE allowed(id) AS ("
        "SELECT l.id FROM locations l WHERE EXISTS(SELECT 1 FROM management_grants g "
        "WHERE g.user_id=?1 AND g.capability=?2 AND g.revoked_at IS NULL AND (g.scope_location_id IS NULL OR g.scope_location_id=l.id)) "
        "UNION SELECT l.id FROM locations l JOIN allowed p ON l.parent_id=p.id) ";
    const std::string from=" FROM transfer_requests t JOIN work_orders o ON o.id=t.order_id "
        "JOIN assignments a ON a.id=t.assignment_id AND a.order_id=t.order_id WHERE ";
    const std::string scope=input.view=="mine"?"t.requested_by=?1":
        "t.status='PENDING' AND a.ended_at IS NULL AND o.status IN ('PENDING_ACCEPT','IN_PROGRESS') AND o.location_id IN (SELECT id FROM allowed)";
    const std::string filter=from+scope+" AND (?3 IS NULL OR o.id=?3)";
    const auto bind=[&](SqliteStatement &q){q.bindInt(1,user);q.bindText(2,cap);if(input.orderId)q.bindInt(3,*input.orderId);else q.bindNull(3);};
    TransferPage result;result.page=input.page;result.pageSize=input.pageSize;
    {auto q=db_.prepare(cte+"SELECT count(*)"+filter);bind(q);q.step();result.total=q.columnInt(0);}
    auto q=db_.prepare(cte+"SELECT t.id,t.order_id,o.order_no,t.assignment_id,t.reason,t.status,t.created_at,o.status,o.version,t.review_reason,t.resolved_at"+
        filter+" ORDER BY t.created_at DESC,t.id DESC LIMIT ?4 OFFSET ?5");
    bind(q);q.bindInt(4,input.pageSize);q.bindInt(5,(input.page-1)*input.pageSize);
    while(q.step()) {
        TransferItem item;item.id=q.columnInt(0);item.orderId=q.columnInt(1);if(!q.columnIsNull(2))item.orderNo=q.columnText(2);
        item.assignmentId=q.columnInt(3);item.reason=q.columnText(4);item.status=q.columnText(5);item.createdAt=q.columnText(6);
        item.orderStatus=q.columnText(7);item.orderVersion=q.columnInt(8);if(!q.columnIsNull(9))item.reviewReason=q.columnText(9);if(!q.columnIsNull(10))item.resolvedAt=q.columnText(10);
        result.items.push_back(std::move(item));
    }
    return result;
}
}
