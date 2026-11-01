#include "repositories/VerificationRepository.h"
#include "models/WorkOrder.h"
namespace campusfix {
VerificationSnapshot VerificationRepository::read(std::int64_t order) {
    auto q=db_.prepare("SELECT a.id,a.version,a.worker_id,o.rework_count,o.repair_summary,a.accepted_at,a.result_submitted_at,(SELECT count(*) FROM assignments n WHERE n.worker_id=a.worker_id AND n.ended_at IS NULL) FROM work_orders o JOIN assignments a ON a.order_id=o.id AND a.ended_at IS NULL WHERE o.id=?1 AND a.end_reason IS NULL AND o.completed_at IS NULL");q.bindInt(1,order);
    if(!q.step()||q.columnIsNull(4)||q.columnIsNull(5)||q.columnIsNull(6))throw WorkOrderError(409,"ORDER_STATUS_CONFLICT","Repair result or current assignment unavailable");
    return {q.columnInt(0),q.columnInt(1),q.columnInt(2),q.columnInt(3),q.columnInt(7),q.columnText(4),q.columnText(5),q.columnText(6)};
}
void VerificationRepository::save(std::int64_t order,std::int64_t reporter,std::int64_t version,const VerificationSnapshot &s,bool approved,const std::string &utc) {
    auto q=db_.prepare("UPDATE work_orders SET status=?1,version=version+1,rework_count=?2,completed_at=?3,updated_at=?4 WHERE id=?5 AND reporter_id=?6 AND status='PENDING_VERIFY' AND version=?7 AND rework_count=?8 AND repair_summary=?9 AND completed_at IS NULL");
    q.bindText(1,approved?"COMPLETED":"IN_PROGRESS");q.bindInt(2,s.reworks+(approved?0:1));if(approved)q.bindText(3,utc);else q.bindNull(3);q.bindText(4,utc);q.bindInt(5,order);q.bindInt(6,reporter);q.bindInt(7,version);q.bindInt(8,s.reworks);q.bindText(9,s.summary);q.step();
    if(sqlite3_changes(db_.nativeHandle())!=1)throw WorkOrderError(409,"ORDER_VERSION_CONFLICT","Order changed");
}
void VerificationRepository::end(std::int64_t order,const VerificationSnapshot &s,const std::string &utc) {
    auto q=db_.prepare("UPDATE assignments SET ended_at=?1,end_reason='COMPLETED',version=version+1 WHERE id=?2 AND order_id=?3 AND worker_id=?4 AND version=?5 AND ended_at IS NULL AND end_reason IS NULL AND accepted_at=?6 AND result_submitted_at=?7");
    q.bindText(1,utc);q.bindInt(2,s.assignment);q.bindInt(3,order);q.bindInt(4,s.worker);q.bindInt(5,s.assignmentVersion);q.bindText(6,s.accepted);q.bindText(7,s.submitted);q.step();
    if(sqlite3_changes(db_.nativeHandle())!=1)throw WorkOrderError(409,"ORDER_STATUS_CONFLICT","Assignment changed");
}
bool VerificationRepository::saved(std::int64_t order,std::int64_t reporter,std::int64_t version,const VerificationSnapshot &s,bool approved,const std::string &utc) {
    auto q=db_.prepare("SELECT 1 FROM work_orders o JOIN assignments a ON a.order_id=o.id WHERE o.id=?1 AND o.reporter_id=?2 AND o.status=?3 AND o.version=?4 AND o.rework_count=?5 AND o.repair_summary=?6 AND o.updated_at=?7 AND o.completed_at IS ?8 AND a.id=?9 AND a.worker_id=?10 AND a.version=?11 AND a.accepted_at=?12 AND a.result_submitted_at=?13 AND a.ended_at IS ?14 AND a.end_reason IS ?15 AND (SELECT count(*) FROM assignments n WHERE n.order_id=o.id AND n.ended_at IS NULL)=?16 AND (SELECT count(*) FROM assignments n WHERE n.worker_id=a.worker_id AND n.ended_at IS NULL)=?17");
    q.bindInt(1,order);q.bindInt(2,reporter);q.bindText(3,approved?"COMPLETED":"IN_PROGRESS");q.bindInt(4,version+1);q.bindInt(5,s.reworks+(approved?0:1));q.bindText(6,s.summary);q.bindText(7,utc);if(approved)q.bindText(8,utc);else q.bindNull(8);q.bindInt(9,s.assignment);q.bindInt(10,s.worker);q.bindInt(11,s.assignmentVersion+(approved?1:0));q.bindText(12,s.accepted);q.bindText(13,s.submitted);if(approved){q.bindText(14,utc);q.bindText(15,"COMPLETED");}else{q.bindNull(14);q.bindNull(15);}q.bindInt(16,approved?0:1);q.bindInt(17,s.load-(approved?1:0));return q.step();
}
}
