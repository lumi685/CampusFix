#include "repositories/WorkerRepository.h"
#include "models/WorkOrder.h"

namespace campusfix {
namespace {
const std::string columns="w.id,w.user_id,w.worker_no,u.display_name,w.service_area_id,w.availability,w.max_concurrent_tasks,"
    "(SELECT count(*) FROM assignments a WHERE a.worker_id=w.id AND a.ended_at IS NULL),w.updated_at,w.active,w.emergency_duty";
WorkerRecord read(SqliteStatement &q) {
    WorkerRecord r;r.id=q.columnInt(0);r.userId=q.columnInt(1);r.workerNo=q.columnText(2);r.displayName=q.columnText(3);
    r.serviceAreaId=q.columnInt(4);r.availability=q.columnText(5);r.capacity=q.columnInt(6);r.activeTasks=q.columnInt(7);r.updatedAt=q.columnText(8);
    r.active=q.columnInt(9)!=0;r.emergencyDuty=q.columnInt(10)!=0;return r;
}
}
std::vector<WorkerSkill> WorkerRepository::skills(std::int64_t id) {
    auto q=db_.prepare("SELECT ws.skill_id,ws.level,ws.certified FROM worker_skills ws JOIN skills s ON s.id=ws.skill_id "
        "WHERE ws.worker_id=?1 AND ws.active=1 AND s.active=1 ORDER BY ws.skill_id");q.bindInt(1,id);std::vector<WorkerSkill> result;
    while(q.step())result.push_back({q.columnInt(0),q.columnInt(1),q.columnInt(2)!=0});return result;
}
WorkerRecord WorkerRepository::find(std::int64_t id) {
    auto q=db_.prepare("SELECT "+columns+" FROM workers w JOIN users u ON u.id=w.user_id WHERE w.id=?1");q.bindInt(1,id);
    if(!q.step())throw WorkOrderError(404,"RESOURCE_NOT_FOUND","Worker not found");
    auto result=read(q);result.skills=skills(id);return result;
}
WorkerPage WorkerRepository::list(std::int64_t user,const std::string &cap,const WorkerQuery &query) {
    const std::string cte="WITH RECURSIVE allowed(id) AS (SELECT scope_location_id FROM management_grants "
        "WHERE user_id=?1 AND capability=?2 AND revoked_at IS NULL AND scope_location_id IS NOT NULL UNION "
        "SELECT l.id FROM locations l JOIN allowed a ON l.parent_id=a.id) ";
    const std::string scope=query.mine?"w.user_id=?1":"(w.service_area_id IN (SELECT id FROM allowed) OR EXISTS("
        "SELECT 1 FROM management_grants g WHERE g.user_id=?1 AND g.capability=?2 AND g.revoked_at IS NULL AND g.scope_location_id IS NULL))";
    const std::string from=" FROM workers w JOIN users u ON u.id=w.user_id WHERE "+scope+
        " AND (?3 IS NULL OR w.availability=?3) AND (?4 IS NULL OR EXISTS(SELECT 1 FROM worker_skills ws JOIN skills s ON s.id=ws.skill_id "
        "WHERE ws.worker_id=w.id AND ws.skill_id=?4 AND ws.active=1 AND s.active=1))";
    const auto bind=[&](SqliteStatement &q){q.bindInt(1,user);q.bindText(2,cap);if(query.availability)q.bindText(3,*query.availability);else q.bindNull(3);if(query.skillId)q.bindInt(4,*query.skillId);else q.bindNull(4);};
    WorkerPage result;result.page=query.page;result.pageSize=query.pageSize;result.management=!query.mine;
    if(result.management) {
        auto options=db_.prepare("SELECT id,code,name,requires_certificate FROM skills WHERE active=1 ORDER BY code,id");
        while(options.step())result.skillOptions.push_back({options.columnInt(0),options.columnText(1),options.columnText(2),options.columnInt(3)!=0});
    }
    {auto q=db_.prepare(cte+"SELECT count(*)"+from);bind(q);q.step();result.total=q.columnInt(0);}
    auto q=db_.prepare(cte+"SELECT "+columns+from+" ORDER BY w.worker_no,w.id LIMIT ?5 OFFSET ?6");bind(q);q.bindInt(5,query.pageSize);q.bindInt(6,(query.page-1)*query.pageSize);
    while(q.step()){auto r=read(q);r.skills=skills(r.id);result.items.push_back(std::move(r));}return result;
}
void WorkerRepository::validateReferences(const WorkerInput &i) {
    auto area=db_.prepare("WITH RECURSIVE chain(id,parent_id,active,location_type) AS (SELECT id,parent_id,active,location_type FROM locations WHERE id=?1 "
        "UNION SELECT l.id,l.parent_id,l.active,l.location_type FROM locations l JOIN chain c ON l.id=c.parent_id) SELECT 1 "
        "WHERE EXISTS(SELECT 1 FROM chain WHERE id=?1 AND location_type IN ('CAMPUS','BUILDING')) AND NOT EXISTS(SELECT 1 FROM chain WHERE active<>1) "
        "AND EXISTS(SELECT 1 FROM chain WHERE parent_id IS NULL)");area.bindInt(1,i.serviceAreaId);
    if(!area.step())throw WorkOrderError(422,"VALIDATION_ERROR","Invalid service area","service_area_id");
    for(std::size_t n=0;n<i.skills.size();n++) {
        auto q=db_.prepare("SELECT id FROM skills WHERE id=?1 AND active=1");q.bindInt(1,i.skills[n].skillId);
        if(!q.step())throw WorkOrderError(422,"VALIDATION_ERROR","Invalid active skill","skills["+std::to_string(n)+"].skill_id");
    }
}
void WorkerRepository::update(std::int64_t id,const WorkerInput &i,const std::string &utc) {
    auto q=db_.prepare("UPDATE workers SET service_area_id=?1,availability=?2,max_concurrent_tasks=?3,emergency_duty=?4,active=?5,updated_at=?6 WHERE id=?7");
    q.bindInt(1,i.serviceAreaId);q.bindText(2,i.availability);q.bindInt(3,i.capacity);q.bindInt(4,i.emergencyDuty?1:0);q.bindInt(5,i.active?1:0);q.bindText(6,utc);q.bindInt(7,id);q.step();
    if(sqlite3_changes(db_.nativeHandle())!=1)throw WorkOrderError(404,"RESOURCE_NOT_FOUND","Worker not found");
    // Do not remove rows or rewrite effective_at for continuously active associations.
    // Include globally inactive skills too: removal must not leave their relationship active.
    auto existing=db_.prepare("SELECT skill_id FROM worker_skills WHERE worker_id=?1 AND active=1");existing.bindInt(1,id);
    std::vector<std::int64_t> removed;while(existing.step()){const auto old=existing.columnInt(0);bool keep=false;for(const auto &requested:i.skills)if(requested.skillId==old)keep=true;if(!keep)removed.push_back(old);}
    for(const auto old:removed){auto off=db_.prepare("UPDATE worker_skills SET active=0,updated_at=?1 WHERE worker_id=?2 AND skill_id=?3");off.bindText(1,utc);off.bindInt(2,id);off.bindInt(3,old);off.step();}
    for(const auto &skill:i.skills) {
        auto s=db_.prepare("INSERT INTO worker_skills(worker_id,skill_id,level,certified,active,effective_at,updated_at) VALUES(?1,?2,?3,?4,1,?5,?5) "
            "ON CONFLICT(worker_id,skill_id) DO UPDATE SET level=excluded.level,certified=excluded.certified,active=1,"
            "effective_at=CASE WHEN worker_skills.active=0 THEN excluded.effective_at ELSE worker_skills.effective_at END,updated_at=excluded.updated_at");
        s.bindInt(1,id);s.bindInt(2,skill.skillId);s.bindInt(3,skill.level);s.bindInt(4,skill.certified?1:0);s.bindText(5,utc);s.step();
    }
}
}
