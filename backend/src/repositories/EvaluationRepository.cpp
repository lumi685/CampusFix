#include "repositories/EvaluationRepository.h"
#include "models/WorkOrder.h"
#include "domain/WorkOrderValidation.h"
#include <sstream>

namespace campusfix {
namespace {
std::string rows(SqliteConnection &db,const char *sql,std::int64_t id,int columns) {
    auto q=db.prepare(sql);q.bindInt(1,id);std::ostringstream out;
    while(q.step()){out<<'[';for(int i=0;i<columns;i++){if(q.columnIsNull(i))out<<"N;";else{const auto v=q.columnText(i);out<<v.size()<<':'<<v<<';';}}out<<']';}
    return out.str();
}
}
std::optional<EvaluationRecord> EvaluationRepository::find(std::int64_t order) {
    auto q=db_.prepare("SELECT id,order_id,reporter_id,score,comment,created_at FROM evaluations WHERE order_id=?1");q.bindInt(1,order);
    if(!q.step())return {};EvaluationRecord e;e.id=q.columnInt(0);e.orderId=q.columnInt(1);e.reporterId=q.columnInt(2);e.score=q.columnInt(3);if(!q.columnIsNull(4))e.comment=q.columnText(4);e.createdAt=q.columnText(5);return e;
}
EvaluationRecord EvaluationRepository::insert(std::int64_t order,std::int64_t reporter,std::int64_t score,const std::optional<std::string> &comment,const std::string &utc) {
    auto q=db_.prepare("INSERT INTO evaluations(order_id,reporter_id,score,comment,created_at) VALUES(?1,?2,?3,?4,?5)");q.bindInt(1,order);q.bindInt(2,reporter);q.bindInt(3,score);if(comment)q.bindText(4,*comment);else q.bindNull(4);q.bindText(5,utc);q.step();
    if(sqlite3_changes(db_.nativeHandle())!=1)throw WorkOrderError(409,"DUPLICATE_OPERATION","Evaluation not inserted");
    const auto saved=find(order);if(!saved)throw WorkOrderError(409,"DUPLICATE_OPERATION","Evaluation unavailable");return *saved;
}
std::string EvaluationRepository::stableOrder(std::int64_t order,bool closing) {
    // Closing changes only status/version/closed_at/updated_at; evaluation changes none.
    return closing?rows(db_,"SELECT id,order_no,reporter_id,category_id,location_id,title,description,requested_urgency,approved_urgency,expected_visit_at,contact_name,contact_phone,repair_summary,rework_count,created_at,submitted_at,completed_at,cancelled_at FROM work_orders WHERE id=?1",order,18):
        rows(db_,"SELECT id,order_no,reporter_id,category_id,location_id,title,description,requested_urgency,approved_urgency,expected_visit_at,contact_name,contact_phone,status,version,repair_summary,rework_count,created_at,updated_at,submitted_at,completed_at,closed_at,cancelled_at FROM work_orders WHERE id=?1",order,22);
}
std::string EvaluationRepository::history(std::int64_t order) {
    return rows(db_,"SELECT id,order_id,worker_id,assigned_by,assign_reason,assigned_at,accepted_at,result_submitted_at,ended_at,end_reason,version FROM assignments WHERE order_id=?1 ORDER BY id",order,11)+
        rows(db_,"SELECT id,order_id,requested_by,selected_worker_id,assignment_id,order_version,strategy,weights_json,candidates_json,excluded_summary_json,created_at FROM dispatch_records WHERE order_id=?1 ORDER BY id",order,11);
}
std::string EvaluationRepository::completionTime(std::int64_t order) {
    auto q=db_.prepare("SELECT o.completed_at FROM work_orders o WHERE o.id=?1 AND o.status='COMPLETED' AND o.closed_at IS NULL AND o.repair_summary IS NOT NULL AND length(o.repair_summary)>0 AND NOT EXISTS(SELECT 1 FROM assignments a WHERE a.order_id=o.id AND a.ended_at IS NULL) AND EXISTS(SELECT 1 FROM assignments a JOIN status_logs l ON l.order_id=o.id AND l.assignment_id=a.id AND l.action='ORDER_VERIFIED' AND l.from_status='PENDING_VERIFY' AND l.to_status='COMPLETED' AND l.created_at=o.completed_at WHERE a.order_id=o.id AND a.end_reason='COMPLETED' AND a.ended_at=o.completed_at AND julianday(a.accepted_at)<=julianday(a.result_submitted_at) AND julianday(a.result_submitted_at)<=julianday(a.ended_at))");q.bindInt(1,order);
    if(!q.step()||q.columnIsNull(0)||!validUtcDateTime(q.columnText(0)))throw WorkOrderError(409,"ORDER_STATUS_CONFLICT","Verified completion evidence unavailable");return q.columnText(0);
}
void EvaluationRepository::close(std::int64_t order,std::int64_t version,const std::string &utc) {
    auto q=db_.prepare("UPDATE work_orders SET status='CLOSED',version=version+1,closed_at=?1,updated_at=?1 WHERE id=?2 AND status='COMPLETED' AND version=?3 AND closed_at IS NULL AND NOT EXISTS(SELECT 1 FROM assignments a WHERE a.order_id=work_orders.id AND a.ended_at IS NULL)");q.bindText(1,utc);q.bindInt(2,order);q.bindInt(3,version);q.step();if(sqlite3_changes(db_.nativeHandle())!=1)throw WorkOrderError(409,"ORDER_VERSION_CONFLICT","Order changed");
}
ClosablePage EvaluationRepository::list(std::int64_t user,const std::string &cap,std::int64_t page,std::int64_t pageSize) {
    // Validate UTC using the same domain validator, including impossible dates and
    // embedded NUL, rather than trusting SQLite's permissive date parser alone.
    const auto valid=[](sqlite3_context *context,int,sqlite3_value **args){
        const auto *s=sqlite3_value_text(args[0]);const auto n=sqlite3_value_bytes(args[0]);
        bool ok=false;try{ok=s&&validUtcDateTime(std::string(reinterpret_cast<const char *>(s),static_cast<std::size_t>(n)));}catch(const std::exception &){}sqlite3_result_int(context,ok?1:0);
    };
    const auto code=sqlite3_create_function_v2(db_.nativeHandle(),"campusfix_valid_utc",1,SQLITE_UTF8|SQLITE_DETERMINISTIC,nullptr,valid,nullptr,nullptr,nullptr);
    if(code!=SQLITE_OK)throw SqliteError(code,"UTC validator unavailable");
    const std::string cte="WITH RECURSIVE allowed(id) AS (SELECT l.id FROM locations l WHERE EXISTS(SELECT 1 FROM management_grants g WHERE g.user_id=?1 AND g.capability=?2 AND g.revoked_at IS NULL AND (g.scope_location_id IS NULL OR g.scope_location_id=l.id)) UNION SELECT l.id FROM locations l JOIN allowed p ON l.parent_id=p.id) ";
    const std::string filter=" FROM work_orders o WHERE o.status='COMPLETED' AND campusfix_valid_utc(o.completed_at)=1 AND NOT EXISTS(SELECT 1 FROM assignments a WHERE a.order_id=o.id AND a.ended_at IS NULL) AND o.location_id IN (SELECT id FROM allowed)";
    const auto bind=[&](SqliteStatement &q){q.bindInt(1,user);q.bindText(2,cap);};ClosablePage result;result.page=page;result.pageSize=pageSize;
    {auto q=db_.prepare(cte+"SELECT count(*)"+filter);bind(q);q.step();result.total=q.columnInt(0);}
    auto q=db_.prepare(cte+"SELECT o.id,o.order_no,o.version,o.updated_at"+filter+" ORDER BY o.updated_at DESC,o.id DESC LIMIT ?3 OFFSET ?4");bind(q);q.bindInt(3,pageSize);q.bindInt(4,(page-1)*pageSize);
    while(q.step()){ClosableItem item;item.id=q.columnInt(0);if(!q.columnIsNull(1))item.orderNo=q.columnText(1);item.version=q.columnInt(2);item.updatedAt=q.columnText(3);result.items.push_back(std::move(item));}return result;
}
}
