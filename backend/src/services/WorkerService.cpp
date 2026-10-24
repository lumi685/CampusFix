#include "services/WorkerService.h"
#include "services/AuthService.h"
#include "services/AuthorizationService.h"
#include "domain/WorkOrderValidation.h"
#include <limits>
#include <set>
#include <algorithm>

namespace campusfix {
namespace {
bool availability(const std::string &s){return s=="AVAILABLE"||s=="BUSY"||s=="OFF_DUTY"||s=="DISABLED";}
void invalid(const std::string &field){throw WorkOrderError(422,"VALIDATION_ERROR","Invalid worker field",field);}
}
void WorkerService::authenticate(std::int64_t user,const std::string &token) {
    try {if(AuthService::me(db_,token).user.id!=user)throw WorkOrderError(401,"AUTH_INVALID","Invalid session");}
    catch(const std::invalid_argument &e){throw WorkOrderError(401,e.what(),"Invalid session");}
}
void WorkerService::authorize(std::int64_t user,std::int64_t worker,bool write) {
    AuthorizationService authorization(db_);const auto d=authorization.worker(user,write?Action::WorkerManage:Action::WorkerRead,worker);
    if(d.allowed())return;
    if(d.status==401)throw WorkOrderError(401,d.code,"Invalid session");
    // A visible own record without write permission is 403; hidden resources stay 404.
    const bool visible=write&&authorization.worker(user,Action::WorkerRead,worker).allowed();
    throw WorkOrderError(visible?403:404,visible?"ACTION_FORBIDDEN":"RESOURCE_NOT_FOUND","Worker operation not permitted");
}
WorkerPage WorkerService::list(std::int64_t user,const std::string &token,const WorkerQuery &q) {
    SqliteTransaction tx(db_,false);authenticate(user,token);const auto subject=AuthorizationRepository(db_).subject(user);
    std::string cap;
    if(q.mine) {if(subject.role!="worker")throw WorkOrderError(403,"ACTION_FORBIDDEN","Only workers may query their own profile");}
    else {
        if(subject.role!="dispatcher"&&subject.role!="sysadmin")throw WorkOrderError(403,"ACTION_FORBIDDEN","Management permission required");
        cap=subject.role=="dispatcher"?"AREA_SCOPE":"WORKER_MANAGE";
        bool grant=false;for(const auto &g:subject.grants)if(g.capability==cap)grant=true;
        if(!grant)throw WorkOrderError(403,"ACTION_FORBIDDEN","Management permission required");
    }
    if(q.page<1)invalid("page");if(q.pageSize<1||q.pageSize>100)invalid("page_size");
    if(q.page-1>std::numeric_limits<std::int64_t>::max()/q.pageSize)invalid("page");
    if(q.availability&&!availability(*q.availability))invalid("availability");if(q.skillId&&*q.skillId<=0)invalid("skill_id");
    const auto result=WorkerRepository(db_).list(user,cap,q);tx.commit();return result;
}
WorkerLoad WorkerService::workload(std::int64_t user,const std::string &token,std::int64_t id) {
    SqliteTransaction tx(db_,false);authenticate(user,token);if(id<=0)invalid("id");authorize(user,id,false);
    const auto worker=WorkerRepository(db_).find(id);
    WorkerLoad result{id,worker.activeTasks,worker.capacity,std::max<std::int64_t>(0,worker.capacity-worker.activeTasks),orderTime(clock_()).utc,worker.activeTasks>worker.capacity};
    tx.commit();return result;
}
WorkerRecord WorkerService::update(std::int64_t user,const std::string &token,std::int64_t id,const WorkerInput &input) {
    SqliteTransaction tx(db_);authenticate(user,token);if(id<=0)invalid("id");authorize(user,id,true);
    if(input.serviceAreaId<=0)invalid("service_area_id");
    if(!availability(input.availability))invalid("availability");if(input.capacity<1||input.capacity>100)invalid("max_concurrent_tasks");
    std::set<std::int64_t> ids;
    for(std::size_t n=0;n<input.skills.size();n++) {
        const auto &s=input.skills[n];const auto prefix="skills["+std::to_string(n)+"]";
        if(s.skillId<=0||!ids.insert(s.skillId).second)invalid(prefix+".skill_id");if(s.level<1||s.level>5)invalid(prefix+".level");
    }
    WorkerRepository repo(db_);repo.validateReferences(input);
    const auto target=AuthorizationService(db_).location(user,Action::WorkerManage,input.serviceAreaId);
    if(!target.allowed())throw WorkOrderError(target.status,target.code,"Target service area not permitted");
    const auto current=repo.find(id);
    if(input.capacity<current.activeTasks)invalid("max_concurrent_tasks");
    repo.update(id,input,orderTime(clock_()).utc);
    const auto result=repo.find(id);tx.commit();return result;
}
}
