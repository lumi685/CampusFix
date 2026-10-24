#include "WorkerTestData.h"
#include "domain/WorkOrderValidation.h"
#include <future>
#include <atomic>
#include <thread>
#include <iostream>

using namespace campusfix;
int main() {
    initializeCrypto();const auto root=std::filesystem::temp_directory_path()/("campusfix-worker-service-"+newOpaqueId());std::filesystem::create_directories(root);int checks=0;
    const auto check=[&](bool ok,const std::string &s){if(!ok)throw std::runtime_error(s);++checks;std::cout<<"PASS SQLite/WorkerService "<<s<<'\n';};
    try {
        const auto path=root/"workers.sqlite3";const auto f=test::workerFixture(path,CAMPUSFIX_MIGRATIONS_DIR,CAMPUSFIX_SEED_FILE);
        {
            SqliteConnection db(path);auto now=std::chrono::system_clock::now();WorkerService service(db,[&]{return now;});
            const auto da=f.accounts.at("demo_dispatcher"),dbb=f.accounts.at("test_dispatcher_b"),admin=f.accounts.at("demo_sysadmin"),worker=f.accounts.at("demo_worker01"),reporter=f.accounts.at("demo_reporter01");
            const auto list=[&](const test::ReadAccount &a,WorkerQuery q=WorkerQuery{}){return service.list(a.id,a.token,q);};
            const auto put=[&](const test::ReadAccount &a,std::int64_t id,const WorkerInput &input){return service.update(a.id,a.token,id,input);};
            const auto reject=[&](const std::function<void()> &call,int status){const auto before=test::workerFingerprint(db);bool caught=false;try{call();}catch(const WorkOrderError &e){caught=e.status==status;}check(caught&&before==test::workerFingerprint(db),"rejected "+std::to_string(status)+" with whole database unchanged");};
            const auto immutable=test::reviewFingerprint(db);
            const auto a=list(da),b=list(dbb);check(a.total==3&&b.total==3&&a.items[0].id==1&&b.items[0].id==4,"A/B scoped list and totals");
            check(a.management&&a.skillOptions.size()==3,"management skill options include unassigned skills independently of page");
            db.execute("UPDATE workers SET active=0,emergency_duty=1,availability='OFF_DUTY' WHERE id=1");
            const auto hydrated=list(da);check(!hydrated.items[0].active&&hydrated.items[0].emergencyDuty&&hydrated.items[0].availability=="OFF_DUTY","real management boolean hydration");
            db.execute("UPDATE workers SET active=1,emergency_duty=0,availability='AVAILABLE' WHERE id=1");
            test::grant(db,"demo_dispatcher","AREA_SCOPE",3);check(list(da).total==3,"overlapping grants do not duplicate workers");
            WorkerQuery q;q.pageSize=2;check(list(da,q).items.size()==2&&list(da,q).total==3,"stable first page and scoped total");q.page=2;check(list(da,q).items.size()==1&&list(da,q).items[0].id==3,"stable second page");q.page=3;check(list(da,q).items.empty(),"empty page retains scoped total");
            q={};q.availability="OFF_DUTY";check(list(da,q).total==1&&list(da,q).items[0].id==3,"availability filter");q.availability="DISABLED";check(list(dbb,q).total==1,"disabled profile stays manageable");
            q={};q.skillId=1;check(list(da,q).total==2,"skill filter uses effective relationship");q.skillId=3;check(list(da,q).items.empty(),"empty authorized skill result");
            q={};q.mine=true;check(list(worker,q).total==1&&list(worker,q).items[0].id==1,"worker mine view only own profile");reject([&]{list(worker);},403);reject([&]{list(reporter);},403);reject([&]{list(admin);},403);
            test::grant(db,"demo_sysadmin","AREA_SCOPE",1);reject([&]{list(admin);},403);test::grant(db,"demo_sysadmin","WORKER_MANAGE",2);check(list(admin).total==3&&list(admin).items[0].id==4,"admin uses WORKER_MANAGE own scope, not AREA_SCOPE");
            reject([&]{service.workload(admin.id,admin.token,1);},404);reject([&]{service.workload(da.id,da.token,4);},404);reject([&]{service.workload(worker.id,worker.token,2);},404);reject([&]{service.workload(reporter.id,reporter.token,1);},404);reject([&]{service.workload(da.id,da.token,999999);},404);
            auto load=service.workload(worker.id,worker.token,1);check(load.activeTasks==2&&load.capacity==3&&load.remaining==1&&validUtcDateTime(load.calculatedAt),"real pending acceptance tasks count, UTC and remaining");
            check(service.workload(da.id,da.token,2).activeTasks==2,"ended historical assignments excluded");check(service.workload(da.id,da.token,3).activeTasks==0,"zero tasks");
            db.execute("UPDATE work_orders SET status='COMPLETED' WHERE id=(SELECT order_id FROM assignments WHERE worker_id=1 AND ended_at IS NULL LIMIT 1)");check(service.workload(da.id,da.token,1).activeTasks==2,"order status never substitutes ended_at");
            db.execute("UPDATE workers SET max_concurrent_tasks=1 WHERE id=1");load=service.workload(da.id,da.token,1);check(load.overloaded&&load.activeTasks==2&&load.remaining==0,"legacy overload preserves true load and diagnostic flag");db.execute("UPDATE workers SET max_concurrent_tasks=3 WHERE id=1");
            // From here on compare business rows/logs after deliberate test setup changes.
            const auto business=test::reviewFingerprint(db);auto input=test::workerInput();
            reject([&]{put(da,1,test::workerInput(2));},403);reject([&]{put(admin,1,test::workerInput(2));},404);reject([&]{put(worker,1,input);},403);
            test::grant(db,"demo_sysadmin","WORKER_MANAGE",1);
            const auto originalEffective=test::scalarText(db,"SELECT effective_at FROM worker_skills WHERE worker_id=1 AND skill_id=1");
            for(const auto *state:{"AVAILABLE","BUSY","OFF_DUTY","DISABLED"}) {input.availability=state;input.active=std::string(state)!="DISABLED";input.emergencyDuty=std::string(state)=="BUSY";const auto result=put(da,1,input);check(result.availability==state&&result.skills.size()==2,"full profile update "+std::string(state));}
            check(test::scalarText(db,"SELECT effective_at FROM worker_skills WHERE worker_id=1 AND skill_id=1")==originalEffective,"continuously active skill preserves effective_at");
            input=test::workerInput();input.skills={{2,4,false}};put(da,1,input);check(db.scalarInt("SELECT active FROM worker_skills WHERE worker_id=1 AND skill_id=1")==0,"omitted old skill deactivated, not deleted");
            now+=std::chrono::seconds(2);input.skills={{1,3,false}};put(da,1,input);check(test::scalarText(db,"SELECT effective_at FROM worker_skills WHERE worker_id=1 AND skill_id=1")!=originalEffective,"reactivation starts new effective time");
            check(db.scalarInt("SELECT certified FROM worker_skills WHERE worker_id=1 AND skill_id=1")==0&&db.scalarInt("SELECT requires_certificate FROM skills WHERE id=1")==1,"certificate requirement retained without inventing qualification");
            input.skills.clear();put(da,1,input);check(WorkerRepository(db).find(1).skills.empty()&&db.scalarInt("SELECT count(*) FROM worker_skills WHERE worker_id=1")==2,"empty effective skill set retains historical rows");
            input=test::workerInput();input.capacity=2;put(da,1,input);check(service.workload(da.id,da.token,1).remaining==0,"capacity equal to current tasks allowed");input.capacity=1;reject([&]{put(da,1,input);},422);
            for(const auto cap:{0,101}){input=test::workerInput();input.capacity=cap;reject([&]{put(da,1,input);},422);}
            for(const auto level:{0,6}){input=test::workerInput();input.skills[0].level=level;reject([&]{put(da,1,input);},422);}
            input=test::workerInput();input.skills.push_back(input.skills[0]);reject([&]{put(da,1,input);},422);
            input=test::workerInput();input.skills[0].skillId=999999;reject([&]{put(da,1,input);},422);
            db.execute("UPDATE skills SET active=0 WHERE id=1");input=test::workerInput();reject([&]{put(da,1,input);},422);db.execute("UPDATE skills SET active=1 WHERE id=1");
            for(const auto area:{999999,7,11}){input=test::workerInput(area);reject([&]{put(da,1,input);},422);}
            db.execute("UPDATE locations SET active=0 WHERE id=3");input=test::workerInput(3);reject([&]{put(da,1,input);},422);db.execute("UPDATE locations SET active=1 WHERE id=3");
            input=test::workerInput(3);put(da,1,input);check(WorkerRepository(db).find(1).serviceAreaId==3,"authorized building service area");
            input=test::workerInput(2);put(admin,1,input);check(WorkerRepository(db).find(1).serviceAreaId==2,"both old and target scopes granted allows move");reject([&]{put(da,1,input);},404);put(admin,1,test::workerInput());
            test::revoke(db,"demo_sysadmin","WORKER_MANAGE");reject([&]{put(admin,1,test::workerInput());},404);test::grant(db,"demo_sysadmin","WORKER_MANAGE",{});
            db.execute("UPDATE users SET status='DISABLED' WHERE username='demo_dispatcher'");reject([&]{put(da,1,test::workerInput());},401);db.execute("UPDATE users SET status='ACTIVE' WHERE username='demo_dispatcher'");
            for(const auto *trigger:{"CREATE TRIGGER fail_worker AFTER UPDATE ON workers BEGIN SELECT RAISE(ABORT,'test worker failure'); END","CREATE TRIGGER fail_worker BEFORE INSERT ON worker_skills WHEN NEW.skill_id=3 BEGIN SELECT RAISE(ABORT,'test second skill failure'); END"}) {
                db.execute(trigger);const auto before=test::workerFingerprint(db);bool failed=false;input=test::workerInput();input.skills={{2,5,true},{3,2,false}};
                try{put(da,1,input);}catch(const SqliteError &){failed=true;}check(failed&&before==test::workerFingerprint(db),"injected profile/second-skill failure rolls back entire transaction");db.execute("DROP TRIGGER fail_worker");
            }
            std::atomic<int> ready{0};auto aInput=test::workerInput(3),bInput=test::workerInput(1);aInput.capacity=2;aInput.availability="BUSY";aInput.skills={{2,5,false}};bInput.capacity=3;bInput.availability="OFF_DUTY";bInput.skills={{3,2,true}};
            const auto race=[&](const WorkerInput &i){SqliteConnection connection(path);++ready;while(ready<2)std::this_thread::yield();return WorkerService(connection).update(admin.id,admin.token,1,i);};
            auto one=std::async(std::launch::async,race,aInput),two=std::async(std::launch::async,race,bInput);one.get();two.get();const auto final=WorkerRepository(db).find(1);
            check((final.capacity==2&&final.availability=="BUSY"&&final.serviceAreaId==3&&final.skills.size()==1&&final.skills[0].skillId==2)||(final.capacity==3&&final.availability=="OFF_DUTY"&&final.serviceAreaId==1&&final.skills.size()==1&&final.skills[0].skillId==3),"concurrent updates serialize into one complete final profile, no optimistic lock claim");
            check(test::reviewFingerprint(db)==business,"profile writes never change orders, assignments or audit history");
            test::integrity(db);check(true,"integrity_check=ok; foreign_key_check empty");(void)immutable;
        }
        std::filesystem::remove_all(root);std::cout<<"PASS "<<checks<<" Worker SQLite/Service assertions; 0 new pure Policy tests\n";return 0;
    }catch(const std::exception &e){std::cerr<<e.what()<<'\n';std::filesystem::remove_all(root);return 1;}
}
