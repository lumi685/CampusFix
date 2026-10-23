#include "infrastructure/DatabaseMigration.h"
#include "infrastructure/DemoSeed.h"
#include "infrastructure/AuthCrypto.h"
#include "services/AuthorizationService.h"
#include <filesystem>
#include <iostream>
#include <stdexcept>

using namespace campusfix;
namespace {
int checks=0;
void expect(const AccessDecision &d,int status,const char *label) {
    if(d.status!=status) throw std::runtime_error(std::string(label)+": expected "+std::to_string(status)+", got "+std::to_string(d.status));
    ++checks;std::cout<<"PASS "<<label<<'\n';
}
void require(bool ok,const char *label) {if(!ok) throw std::runtime_error(label);++checks;}
std::int64_t lookup(SqliteConnection &db,const char *table,const char *key,const std::string &value) {
    auto q=db.prepare(std::string("SELECT id FROM ")+table+" WHERE "+key+"=?1");q.bindText(1,value);
    if(!q.step()) throw std::runtime_error("Missing test fixture");return q.columnInt(0);
}
}
int main() {
    initializeCrypto();
    const auto root=std::filesystem::temp_directory_path()/("campusfix-policy-"+newOpaqueId());
    try {
        std::filesystem::create_directories(root);const auto path=root/"policy.sqlite3";
        applyMigrations(path,CAMPUSFIX_MIGRATIONS_DIR);applyDemoSeed(path,CAMPUSFIX_SEED_FILE);
        {
        SqliteConnection db(path);db.execute("UPDATE users SET status='ACTIVE'");
        const auto reporter=lookup(db,"users","username","demo_reporter01");
        const auto other=lookup(db,"users","username","demo_reporter02");
        const auto worker=lookup(db,"users","username","demo_worker01");
        const auto former=lookup(db,"users","username","demo_worker02");
        const auto dispatcher=lookup(db,"users","username","demo_dispatcher");
        const auto admin=lookup(db,"users","username","demo_sysadmin");
        const auto orderA=lookup(db,"work_orders","order_no","WX202001010001");
        const auto orderB=lookup(db,"work_orders","order_no","WX202001010002");
        const auto self=lookup(db,"work_orders","order_no","WX202001010004");
        const auto campusA=lookup(db,"locations","code","DEMO-CAMPUS-A");
        const auto roomA=lookup(db,"locations","code","DEMO-A1-101");
        const auto buildingA=lookup(db,"locations","code","DEMO-A1");
        const auto workerProfile=lookup(db,"workers","worker_no","DEMO-W001");
        // Current/historical assignments only exist in this isolated fixture.
        auto insert=db.prepare("INSERT INTO assignments(order_id,worker_id,assigned_by,ended_at,end_reason) "
            "SELECT ?1,id,?3,'2020-01-01T00:00:00.000Z','TRANSFERRED' FROM workers WHERE user_id=?2");
        insert.bindInt(1,orderA);insert.bindInt(2,former);insert.bindInt(3,dispatcher);insert.step();
        auto current=db.prepare("INSERT INTO assignments(order_id,worker_id,assigned_by) SELECT ?1,id,?3 FROM workers WHERE user_id=?2");
        current.bindInt(1,orderA);current.bindInt(2,worker);current.bindInt(3,dispatcher);current.step();
        AuthorizationService auth(db);
        expect(auth.order(0,Action::Read,orderA),401,"anonymous policy");
        expect(auth.order(reporter,Action::Read,orderA),200,"reporter reads own");
        expect(auth.order(reporter,Action::Read,orderB),404,"other reporter hidden");
        expect(auth.order(worker,Action::Complete,orderA),200,"current worker allowed");
        expect(auth.order(other,Action::Complete,orderA),404,"unrelated worker operation hidden");
        expect(auth.order(former,Action::Read,orderA),200,"historical minimal read");
        expect(auth.order(former,Action::Complete,orderA),403,"historical assignment cannot operate");
        expect(auth.order(worker,Action::Complete,orderB),404,"non-assigned worker denied");
        expect(auth.order(worker,Action::Create,0),200,"worker own reporting role");
        expect(auth.order(admin,Action::Create,0),403,"admin cannot report by proxy");
        expect(auth.order(reporter,Action::Review,orderA),403,"visible resource action forbidden");
        expect(auth.order(dispatcher,Action::Review,orderA),200,"dispatcher campus descendants");
        expect(auth.order(dispatcher,Action::Read,orderB),404,"dispatcher outside area hidden");
        expect(auth.order(admin,Action::Read,orderB),200,"admin explicit business read");
        expect(auth.order(admin,Action::Review,orderB),403,"admin no implicit review");
        expect(auth.order(admin,Action::Read,orderA),404,"admin correct capability wrong scope");
        expect(auth.order(dispatcher,Action::InternalNotes,orderA),200,"dispatcher internal note in scope");
        expect(auth.order(reporter,Action::InternalNotes,orderA),403,"reporter no internal note");
        expect(auth.order(worker,Action::InternalNotes,orderA),403,"worker no internal note");
        expect(auth.order(admin,Action::InternalNotes,orderB),200,"admin internal plus business scope");
        require(auth.fields(admin,orderB).allows("internal_note"),"authorized internal field missing");
        require(!auth.fields(admin,orderA).allows("contact_phone"),"out-of-scope contact leak");
        db.execute("UPDATE management_grants SET revoked_at='2026-01-01T00:00:00.000Z' WHERE capability='BUSINESS_READ'");
        expect(auth.order(admin,Action::InternalNotes,orderB),404,"internal grant alone not enough");
        db.execute("UPDATE management_grants SET revoked_at=NULL WHERE capability='BUSINESS_READ'");
        auto grant=db.prepare("INSERT INTO management_grants(user_id,capability,scope_location_id,granted_by) VALUES(?1,'REVIEW',?2,?1)");
        grant.bindInt(1,admin);grant.bindInt(2,campusA);grant.step();
        expect(auth.order(admin,Action::Review,orderA),200,"admin review own capability scope");
        expect(auth.order(admin,Action::Review,orderB),403,"read scope cannot substitute review scope");
        db.execute("UPDATE management_grants SET revoked_at='2026-01-01T00:00:00.000Z' WHERE capability='REVIEW'");
        expect(auth.order(admin,Action::Review,orderA),404,"revocation immediately effective");
        db.execute("UPDATE management_grants SET revoked_at='2026-01-01T00:00:00.000Z' WHERE capability='AREA_SCOPE'");
        expect(auth.order(dispatcher,Action::Review,orderA),404,"dispatcher scope revocation effective");
        auto area=db.prepare("INSERT INTO management_grants(user_id,capability,scope_location_id,granted_by) VALUES(?1,'AREA_SCOPE',?2,?1)");
        area.bindInt(1,admin);area.bindInt(2,campusA);area.step();
        expect(auth.order(admin,Action::Review,orderA),404,"sysadmin AREA_SCOPE no business grant");
        // D-017: resource identity, independent from current repair assignment.
        db.execute("UPDATE work_orders SET status='PENDING_VERIFY'");
        expect(auth.operation(reporter,Action::VerifyPass,orderA,1),200,"D017 reporter verifies own");
        expect(auth.operation(worker,Action::VerifyPass,self,1),200,"D017 worker verifies own");
        expect(auth.operation(worker,Action::VerifyFail,self,1),200,"D017 worker requests own rework");
        expect(auth.operation(worker,Action::VerifyPass,orderA,1),403,"D017 current repair assignee cannot verify others");
        expect(auth.operation(worker,Action::VerifyFail,orderA,1),403,"D017 current assignee cannot rework others");
        expect(auth.operation(admin,Action::VerifyPass,orderB,1),403,"D017 manager cannot verify");
        expect(auth.operation(worker,Action::VerifyPass,self,999),409,"D017 version still enforced");
        db.execute("UPDATE work_orders SET status='COMPLETED'");
        expect(auth.operation(worker,Action::Evaluate,self,1),200,"D017 worker evaluates own completed order");
        expect(auth.operation(worker,Action::Evaluate,orderA,1),403,"D017 assignee cannot evaluate others");
        expect(auth.operation(admin,Action::Evaluate,orderB,1),403,"D017 manager cannot evaluate");
        expect(auth.operation(worker,Action::VerifyPass,self,1),409,"D017 invalid state rejected");
        auto evaluation=db.prepare("INSERT INTO evaluations(order_id,reporter_id,score) VALUES(?1,?2,5)");
        evaluation.bindInt(1,self);evaluation.bindInt(2,worker);evaluation.step();
        expect(auth.operation(worker,Action::Evaluate,self,1),409,"D017 duplicate evaluation rejected");
        db.execute("UPDATE work_orders SET status='CLOSED'");
        expect(auth.operation(worker,Action::Evaluate,self,1),409,"D017 closed order cannot evaluate");
        const auto historical=auth.fields(former,orderA);const auto own=auth.fields(worker,self);
        require(historical.allows("public_note")&&!historical.allows("contact_phone"),"historical fields");
        require(own.allows("contact_phone")&&!own.allows("internal_note"),"own fields");
        for(const auto *field:{"password_hash","token_hash","stored_path","metadata_json","unrelated_phone"})
            require(!auth.fields(admin,orderB).allows(field),"sensitive field allowed");
        auto &repo=db;
        const auto ancestors=AuthorizationRepository(repo).ancestors(roomA);
        require(ancestors.size()==4,"campus/building/floor/room ancestors");
        Subject s{admin,"sysadmin",true,{{"REVIEW",false,roomA}}};
        require(AuthorizationPolicy::capability(s,"REVIEW",ancestors),"room covers self");
        require(!AuthorizationPolicy::capability(s,"REVIEW",{buildingA,campusA}),"child scope cannot cover parent");
        s.grants={{"REVIEW",true,0}};
        require(AuthorizationPolicy::capability(s,"REVIEW",ancestors),"explicit global scope");
        expect(auth.worker(worker,Action::WorkerRead,workerProfile),200,"worker own profile");
        expect(auth.worker(worker,Action::WorkerManage,workerProfile),403,"worker cannot maintain profile");
        db.execute("UPDATE users SET status='DISABLED' WHERE role='worker'");
        expect(auth.order(worker,Action::Read,self),401,"disabled subject rejected");
        require(db.scalarInt("SELECT COUNT(*) FROM pragma_foreign_key_check")==0,"foreign key integrity");
        }
        std::filesystem::remove_all(root);
        std::cout<<"PASS "<<checks<<" authorization checks (SQLite/Policy, not business HTTP)\n";return 0;
    } catch(const std::exception &e) {std::cerr<<e.what()<<'\n';return 1;}
}
