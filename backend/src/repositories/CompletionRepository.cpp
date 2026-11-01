#include "repositories/CompletionRepository.h"
#include "models/WorkOrder.h"
namespace campusfix {
void CompletionRepository::save(std::int64_t order,std::int64_t version,const std::string &summary,const std::string &utc) {
    auto q=db_.prepare("UPDATE work_orders SET repair_summary=?1,status='PENDING_VERIFY',version=version+1,updated_at=?2 WHERE id=?3 AND version=?4 AND status='IN_PROGRESS'");
    q.bindText(1,summary);q.bindText(2,utc);q.bindInt(3,order);q.bindInt(4,version);q.step();
    if(sqlite3_changes(db_.nativeHandle())!=1)throw WorkOrderError(409,"ORDER_VERSION_CONFLICT","Order changed");
}
void CompletionRepository::submit(const TransferAssignment &a,std::int64_t order,std::int64_t user,const std::string &utc) {
    auto q=db_.prepare("UPDATE assignments SET result_submitted_at=?1,version=version+1 WHERE id=?2 AND order_id=?3 AND version=?4 AND ended_at IS NULL AND end_reason IS NULL AND accepted_at IS NOT NULL AND julianday(accepted_at)<=julianday(?1) AND (result_submitted_at IS NULL OR julianday(result_submitted_at)<=julianday(?1)) AND EXISTS(SELECT 1 FROM workers w WHERE w.id=assignments.worker_id AND w.user_id=?5)");
    q.bindText(1,utc);q.bindInt(2,a.id);q.bindInt(3,order);q.bindInt(4,a.version);q.bindInt(5,user);q.step();
    if(sqlite3_changes(db_.nativeHandle())!=1)throw WorkOrderError(409,"ORDER_STATUS_CONFLICT","Assignment or timestamps changed");
}
void CompletionRepository::supersede(std::int64_t order,std::int64_t assignment,const std::string &utc) {
    auto count=db_.prepare("SELECT count(*) FROM transfer_requests WHERE order_id=?1 AND assignment_id=?2 AND status='PENDING'");count.bindInt(1,order);count.bindInt(2,assignment);count.step();const auto expected=count.columnInt(0);
    auto q=db_.prepare("UPDATE transfer_requests SET status='SUPERSEDED',reviewed_by=NULL,review_reason='Repair result submitted; pending transfer superseded',resolved_at=?1 WHERE order_id=?2 AND assignment_id=?3 AND status='PENDING'");
    q.bindText(1,utc);q.bindInt(2,order);q.bindInt(3,assignment);q.step();
    if(sqlite3_changes(db_.nativeHandle())!=expected)throw WorkOrderError(409,"ORDER_STATUS_CONFLICT","Transfer changed");
}
bool CompletionRepository::saved(std::int64_t order,std::int64_t version,const TransferAssignment &a,const std::string &summary,const std::string &utc) {
    auto q=db_.prepare("SELECT 1 FROM work_orders o JOIN assignments a ON a.order_id=o.id JOIN workers w ON w.id=a.worker_id WHERE o.id=?1 AND o.status='PENDING_VERIFY' AND o.version=?2 AND o.repair_summary=?3 AND o.updated_at=?4 AND a.id=?5 AND a.version=?6 AND a.ended_at IS NULL AND a.end_reason IS NULL AND a.result_submitted_at=?4 AND w.user_id=?7 AND NOT EXISTS(SELECT 1 FROM transfer_requests t WHERE t.order_id=o.id AND t.assignment_id=a.id AND t.status='PENDING')");
    q.bindInt(1,order);q.bindInt(2,version);q.bindText(3,summary);q.bindText(4,utc);q.bindInt(5,a.id);q.bindInt(6,a.version+1);q.bindInt(7,a.userId);return q.step();
}
}
