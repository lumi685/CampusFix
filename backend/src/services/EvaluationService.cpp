#include "services/EvaluationService.h"
#include "services/WorkflowSupport.h"
#include "repositories/StatusLogRepository.h"
#include "repositories/WorkOrderRepository.h"
#include <limits>

namespace campusfix {
namespace {
void unchanged(bool value){if(!value)throw WorkOrderError(409,"ORDER_STATUS_CONFLICT","Transaction invariants changed");}
void audit(SqliteConnection &db,std::int64_t id,std::int64_t user,const char *action,const char *to,const std::optional<std::string> &note,const std::string &utc,const std::string &rid) {
    StatusLogEntry e;e.orderId=id;e.actorId=user;e.action=action;e.fromStatus="COMPLETED";e.toStatus=to;e.publicNote=note;e.createdAt=utc;e.requestId=rid;StatusLogRepository(db).append(e);
    unchanged(sqlite3_changes(db.nativeHandle())==1);
    auto q=db.prepare("SELECT count(*) FROM status_logs WHERE order_id=?1 AND actor_id=?2 AND action=?3 AND from_status='COMPLETED' AND to_status=?4 AND public_note IS ?5 AND internal_note IS NULL AND metadata_json IS NULL AND created_at=?6 AND request_id=?7");q.bindInt(1,id);q.bindInt(2,user);q.bindText(3,action);q.bindText(4,to);if(note)q.bindText(5,*note);else q.bindNull(5);q.bindText(6,utc);q.bindText(7,rid);q.step();unchanged(q.columnInt(0)==1);
}
bool same(const std::optional<EvaluationRecord> &a,const std::optional<EvaluationRecord> &b){return (!a&&!b)||(a&&b&&a->id==b->id&&a->orderId==b->orderId&&a->reporterId==b->reporterId&&a->score==b->score&&a->comment==b->comment&&a->createdAt==b->createdAt);}
}
EvaluationRecord EvaluationService::create(std::int64_t user,const std::string &token,std::int64_t id,std::int64_t version,std::int64_t score,const std::optional<std::string> &comment,const std::string &rid) {
    if(id<=0||version<=0)throw WorkOrderError(422,"VALIDATION_ERROR","Expected positive int64");
    SqliteTransaction tx(db_);workflow::authenticate(db_,user,token);AuthorizationService auth(db_);workflow::require(auth.order(user,Action::Evaluate,id));workflow::require(auth.operation(user,Action::Evaluate,id,version));
    if(score<1||score>5)throw WorkOrderError(422,"VALIDATION_ERROR","Score must be 1..5","score");if(comment)workflow::note(*comment,1000,"comment",true);
    EvaluationRepository repo(db_);if(repo.find(id))throw WorkOrderError(409,"DUPLICATE_OPERATION","Order already evaluated");const auto order=repo.stableOrder(id,false),history=repo.history(id),utc=orderTime(clock_()).utc;
    const auto result=repo.insert(id,user,score,comment,utc);unchanged(result.orderId==id&&result.reporterId==user&&result.score==score&&result.comment==comment&&result.createdAt==utc);
    audit(db_,id,user,"EVALUATION_CREATED","COMPLETED",{},utc,rid);
    workflow::authenticate(db_,user,token);workflow::require(auth.order(user,Action::Evaluate,id));unchanged(repo.stableOrder(id,false)==order&&repo.history(id)==history&&same(repo.find(id),result));tx.commit();return result;
}
std::optional<EvaluationRecord> EvaluationService::read(std::int64_t user,const std::string &token,std::int64_t id) {
    if(id<=0)throw WorkOrderError(422,"VALIDATION_ERROR","Expected positive int64","id");SqliteTransaction tx(db_,false);workflow::authenticate(db_,user,token);workflow::require(AuthorizationService(db_).order(user,Action::Read,id));const auto e=EvaluationRepository(db_).find(id);tx.commit();return e;
}
CloseResult EvaluationService::close(std::int64_t user,const std::string &token,std::int64_t id,std::int64_t version,const std::optional<std::string> &note,const std::string &rid) {
    if(id<=0||version<=0)throw WorkOrderError(422,"VALIDATION_ERROR","Expected positive int64");SqliteTransaction tx(db_);workflow::authenticate(db_,user,token);AuthorizationService auth(db_);workflow::require(auth.order(user,Action::Close,id));workflow::require(auth.operation(user,Action::Close,id,version));if(note)workflow::note(*note,500,"note",true);
    if(version==std::numeric_limits<std::int64_t>::max())throw WorkOrderError(409,"ORDER_VERSION_CONFLICT","Version limit reached");EvaluationRepository repo(db_);const auto completed=repo.completionTime(id),utc=orderTime(clock_()).utc;
    auto time=db_.prepare("SELECT julianday(?1)<=julianday(?2)");time.bindText(1,completed);time.bindText(2,utc);time.step();unchanged(!time.columnIsNull(0)&&time.columnInt(0)==1);
    const auto order=repo.stableOrder(id,true),history=repo.history(id);const auto evaluation=repo.find(id);repo.close(id,version,utc);audit(db_,id,user,"ORDER_CLOSED","CLOSED",note,utc,rid);
    workflow::authenticate(db_,user,token);workflow::require(auth.order(user,Action::Close,id));auto q=db_.prepare("SELECT 1 FROM work_orders WHERE id=?1 AND status='CLOSED' AND version=?2 AND closed_at=?3 AND updated_at=?3 AND NOT EXISTS(SELECT 1 FROM assignments WHERE order_id=?1 AND ended_at IS NULL)");q.bindInt(1,id);q.bindInt(2,version+1);q.bindText(3,utc);unchanged(q.step()&&order==repo.stableOrder(id,true)&&history==repo.history(id)&&same(evaluation,repo.find(id)));tx.commit();return {id,version+1,utc};
}
ClosablePage EvaluationService::list(std::int64_t user,const std::string &token,std::int64_t page,std::int64_t pageSize) {
    db_.execute("PRAGMA query_only=ON");SqliteTransaction tx(db_,false);workflow::authenticate(db_,user,token);
    if(page<1||pageSize<1||pageSize>100||(page-1)>std::numeric_limits<std::int64_t>::max()/pageSize)throw WorkOrderError(422,"VALIDATION_ERROR","Invalid pagination","page");
    const auto subject=AuthorizationRepository(db_).subject(user);const std::string cap=subject.role=="dispatcher"?"AREA_SCOPE":subject.role=="sysadmin"?"CLOSE":"";bool granted=false;
    for(const auto &g:subject.grants)if(!cap.empty()&&g.capability==cap)granted=true;if(!granted)throw WorkOrderError(403,"ACTION_FORBIDDEN","View unavailable");
    const auto result=EvaluationRepository(db_).list(user,cap,page,pageSize);tx.commit();return result;
}
}
