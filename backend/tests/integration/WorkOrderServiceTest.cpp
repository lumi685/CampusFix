#include "services/WorkOrderService.h"
#include "services/AuthService.h"
#include "infrastructure/AuthCrypto.h"
#include "infrastructure/DatabaseMigration.h"
#include "infrastructure/DemoSeed.h"
#include "domain/WorkOrderValidation.h"
#include <atomic>
#include <filesystem>
#include <future>
#include <iostream>
#include <thread>

using namespace campusfix;
namespace {
int checks=0;
void check(bool ok,const char *name) {
    if(!ok) throw std::runtime_error(name);
    ++checks;std::cout<<"PASS "<<name<<'\n';
}
template<class F> void rejects(F f,int status,const std::string &code) {
    try {f();} catch(const WorkOrderError &e) {check(e.status==status&&e.code==code,"expected business rejection");return;}
    throw std::runtime_error("Expected business rejection");
}
template<class F> void failure(F f) {
    try {f();}catch(const std::exception &) {++checks;return;}
    throw std::runtime_error("Expected injected failure");
}
struct Account {std::int64_t id;std::string token;};
Account account(SqliteConnection &db,const std::string &name) {
    auto enable=db.prepare("UPDATE users SET status='ACTIVE' WHERE username=?1");enable.bindText(1,name);enable.step();
    AuthRepository repo(db);const auto user=repo.userByName(name);auto token=newToken();
    repo.createSession(newOpaqueId(),user.id,tokenDigest(token),"2099-01-01T00:00:00.000Z");
    return {user.id,token};
}
}
int main() {
    const auto root=std::filesystem::temp_directory_path()/("campusfix-orders-"+newOpaqueId());
    try {
        initializeCrypto();std::filesystem::create_directories(root);const auto path=root/"orders.sqlite3";
        applyMigrations(path,CAMPUSFIX_MIGRATIONS_DIR);applyDemoSeed(path,CAMPUSFIX_SEED_FILE);
        {
        SqliteConnection db(path);WorkOrderRepository repo(db);
        const auto r=account(db,"demo_reporter01"),other=account(db,"demo_reporter02"),w=account(db,"demo_worker01"),
                   d=account(db,"demo_dispatcher"),a=account(db,"demo_sysadmin");
        auto moment=std::chrono::system_clock::from_time_t(0)+std::chrono::hours(10);
        WorkOrderService svc(db,[&]{return moment;});
        WorkOrderInput input{db.scalarInt("SELECT id FROM fault_categories WHERE code='DEMO-SOCKET'"),
            db.scalarInt("SELECT id FROM locations WHERE code='DEMO-A1-101'"),
            "Fictional repair","Fictional description","NORMAL","Fictional contact","00000000000",{}};
        auto create=[&](const Account &who){return svc.create(who.id,who.token,input,"test-create");};
        const auto draft=create(r);check(draft.status=="DRAFT"&&draft.version==1&&!draft.orderNo&&!draft.submittedAt,"draft invariants");
        check(create(w).reporterId==w.id,"worker own report");
        rejects([&]{create(d);},403,"ACTION_FORBIDDEN");rejects([&]{create(a);},403,"ACTION_FORBIDDEN");
        auto logs=db.prepare("SELECT actor_id,action,from_status,to_status,request_id FROM status_logs WHERE order_id=?1");
        logs.bindInt(1,draft.id);check(logs.step()&&logs.columnInt(0)==r.id&&logs.columnText(1)=="ORDER_CREATED"&&
            logs.columnIsNull(2)&&logs.columnText(3)=="DRAFT"&&logs.columnText(4)=="test-create"&&!logs.step(),"atomic creation log");
        rejects([&]{svc.submit(other.id,other.token,draft.id,1,"x");},404,"RESOURCE_NOT_FOUND");
        rejects([&]{svc.submit(w.id,w.token,draft.id,1,"x");},404,"RESOURCE_NOT_FOUND");
        rejects([&]{svc.submit(d.id,d.token,draft.id,1,"x");},403,"ACTION_FORBIDDEN");
        rejects([&]{svc.submit(a.id,a.token,draft.id,1,"x");},404,"RESOURCE_NOT_FOUND");
        rejects([&]{svc.submit(r.id,r.token,draft.id,2,"x");},409,"ORDER_VERSION_CONFLICT");
        auto submitted=svc.submit(r.id,r.token,draft.id,1,"submit-rid");
        check(submitted.orderNo=="WX197001010001"&&submitted.status=="PENDING_REVIEW"&&submitted.version==2&&
              submitted.submittedAt=="1970-01-01T10:00:00.000Z","number, state, version and UTC timestamp");
        rejects([&]{svc.submit(r.id,r.token,draft.id,2,"x");},409,"ORDER_STATUS_CONFLICT");
        auto submitLog=db.prepare("SELECT actor_id,action,from_status,to_status,request_id FROM status_logs WHERE order_id=?1 AND action='ORDER_SUBMITTED'");
        submitLog.bindInt(1,draft.id);check(submitLog.step()&&submitLog.columnInt(0)==r.id&&submitLog.columnText(2)=="DRAFT"&&
            submitLog.columnText(3)=="PENDING_REVIEW"&&submitLog.columnText(4)=="submit-rid"&&!submitLog.step(),"single correctly correlated submit log");
        const auto orderCount=db.scalarInt("SELECT count(*) FROM work_orders");
        for(int i=0;i<10;++i) {
            auto bad=input;
            switch(i) {
            case 0:bad.title="";break;case 1:bad.title=std::string(121,'a');break;
            case 2:bad.description=std::string(2001,'a');break;case 3:bad.contactName=std::string(81,'a');break;
            case 4:bad.contactPhone=std::string(33,'1');break;case 5:bad.urgency="INVALID";break;
            case 6:bad.expectedVisitAt="2026-02-30T10:00:00Z";break;case 7:bad.title=std::string("a\0b",3);break;
            case 8:bad.title=std::string("\xED\xA0\x80",3);break;case 9:bad.description=" \n\t";break;
            }
            rejects([&]{svc.create(r.id,r.token,bad,"invalid");},422,"VALIDATION_ERROR");
        }
        for(const auto *field:{"category","location"}) {
            auto bad=input;if(std::string(field)=="category") bad.categoryId=999999;else bad.locationId=999999;
            rejects([&]{svc.create(r.id,r.token,bad,"invalid");},422,"VALIDATION_ERROR");
        }
        for(const auto *table:{"fault_categories","locations"}) {
            const auto id=std::string(table)=="locations"?input.locationId:input.categoryId;
            auto off=db.prepare(std::string("UPDATE ")+table+" SET active=0 WHERE id=?1");off.bindInt(1,id);off.step();
            rejects([&]{create(r);},422,"VALIDATION_ERROR");
            auto on=db.prepare(std::string("UPDATE ")+table+" SET active=1 WHERE id=?1");on.bindInt(1,id);on.step();
        }
        check(db.scalarInt("SELECT count(*) FROM work_orders")==orderCount,"invalid creates leave no rows");
        check(validUtcDateTime("2024-02-29T23:59:59.123456789Z")&&!validUtcDateTime("2023-02-29T00:00:00Z")&&
            !validUtcDateTime("2026-01-01T24:00:00Z")&&!validUtcDateTime("2026-01-01T00:00:00+08:00"),"UTC calendar validation");
        auto unicode=input;unicode.title=std::string{};for(int i=0;i<120;++i) unicode.title+="\xE4\xBF\xAE";
        svc.create(r.id,r.token,unicode,"unicode");unicode.title+="\xE4\xBF\xAE";
        rejects([&]{svc.create(r.id,r.token,unicode,"unicode");},422,"VALIDATION_ERROR");
        const auto rollbackDraft=create(r);
        const auto seqBefore=db.scalarInt("SELECT sum(last_value) FROM order_number_sequences");
        const auto logBefore=db.scalarInt("SELECT count(*) FROM status_logs");
        db.execute("CREATE TRIGGER fail_submit BEFORE INSERT ON status_logs WHEN NEW.action='ORDER_SUBMITTED' BEGIN SELECT RAISE(ABORT,'injected'); END");
        failure([&]{svc.submit(r.id,r.token,rollbackDraft.id,1,"failed");});
        check(repo.find(rollbackDraft.id).status=="DRAFT"&&!repo.find(rollbackDraft.id).orderNo&&
              db.scalarInt("SELECT sum(last_value) FROM order_number_sequences")==seqBefore&&
              db.scalarInt("SELECT count(*) FROM status_logs")==logBefore,"submit fault rolls back order, existing sequence and log");
        moment+=std::chrono::hours(24);
        failure([&]{svc.submit(r.id,r.token,rollbackDraft.id,1,"failed-new-date");});
        check(db.scalarInt("SELECT count(*) FROM order_number_sequences WHERE business_date='19700102'")==0,"failed first daily allocation is rolled back");
        db.execute("DROP TRIGGER fail_submit");
        auto secondDay=svc.submit(r.id,r.token,rollbackDraft.id,1,"second-day");
        check(secondDay.orderNo=="WX197001020001","new business date starts at 0001");
        const auto beforeCreate=db.scalarInt("SELECT count(*) FROM work_orders");
        db.execute("CREATE TRIGGER fail_create BEFORE INSERT ON status_logs WHEN NEW.action='ORDER_CREATED' BEGIN SELECT RAISE(ABORT,'injected'); END");
        failure([&]{create(r);});check(db.scalarInt("SELECT count(*) FROM work_orders")==beforeCreate,"create log fault leaves no orphan");
        db.execute("DROP TRIGGER fail_create");
        auto returnOrder=db.prepare("UPDATE work_orders SET status='PENDING_SUPPLEMENT',version=3 WHERE id=?1");
        returnOrder.bindInt(1,draft.id);returnOrder.step();
        auto resubmitted=svc.submit(r.id,r.token,draft.id,3,"resubmit");
        check(resubmitted.orderNo==submitted.orderNo&&resubmitted.submittedAt==submitted.submittedAt&&resubmitted.version==4,"supplement retains number and first submission time");
        // Submission must revalidate references, not only creation.
        auto invalidDraft=create(r);db.execute("UPDATE fault_categories SET active=0 WHERE code='DEMO-SOCKET'");
        rejects([&]{svc.submit(r.id,r.token,invalidDraft.id,1,"off");},422,"VALIDATION_ERROR");
        db.execute("UPDATE fault_categories SET active=1 WHERE code='DEMO-SOCKET'");
        db.execute("UPDATE locations SET active=0 WHERE code='DEMO-A1-101'");
        rejects([&]{svc.submit(r.id,r.token,invalidDraft.id,1,"off-location");},422,"VALIDATION_ERROR");
        db.execute("UPDATE locations SET active=1 WHERE code='DEMO-A1-101'");
        auto missingContact=db.prepare("UPDATE work_orders SET contact_name='' WHERE id=?1");missingContact.bindInt(1,invalidDraft.id);missingContact.step();
        rejects([&]{svc.submit(r.id,r.token,invalidDraft.id,1,"missing");},422,"VALIDATION_ERROR");
        auto closed=db.prepare("UPDATE work_orders SET status='CLOSED' WHERE id=?1");closed.bindInt(1,invalidDraft.id);closed.step();
        rejects([&]{svc.submit(r.id,r.token,invalidDraft.id,1,"terminal");},409,"ORDER_STATUS_CONFLICT");
        const auto same=create(r),different=create(r);
        auto race=[&](std::int64_t left,std::int64_t right) {
            std::atomic<int> ready{0};
            auto job=[&](std::int64_t id) {
                SqliteConnection connection(path);WorkOrderService service(connection,[&]{return moment;});
                ready.fetch_add(1);while(ready.load()<2) std::this_thread::yield();
                try {return service.submit(r.id,r.token,id,1,"race").status=="PENDING_REVIEW"?200:500;}
                catch(const WorkOrderError &e) {return e.status;}
            };
            auto f=std::async(std::launch::async,job,left),g=std::async(std::launch::async,job,right);
            return std::make_pair(f.get(),g.get());
        };
        const auto sameRace=race(same.id,same.id);
        check((sameRace.first==200&&sameRace.second==409)||(sameRace.second==200&&sameRace.first==409),"two independent connections compete for one draft: exactly one commit");
        auto raceLog=db.prepare("SELECT count(*) FROM status_logs WHERE order_id=?1 AND action='ORDER_SUBMITTED'");
        raceLog.bindInt(1,same.id);check(raceLog.step()&&raceLog.columnInt(0)==1,"race adds exactly one submit event");raceLog.step();
        const auto another=create(r);const auto differentRace=race(different.id,another.id);
        check(differentRace.first==200&&differentRace.second==200&&repo.find(different.id).orderNo!=repo.find(another.id).orderNo,"different drafts submit concurrently without duplicate numbers");
        const auto overflow=create(r);db.execute("UPDATE order_number_sequences SET last_value=9999 WHERE business_date='19700102'");
        rejects([&]{svc.submit(r.id,r.token,overflow.id,1,"overflow");},500,"INTERNAL_ERROR");
        check(!repo.find(overflow.id).orderNo&&repo.find(overflow.id).status=="DRAFT","daily capacity does not create five digit sequence");
        check(orderTime(std::chrono::system_clock::from_time_t(57599)).businessDate=="19700101"&&
            orderTime(std::chrono::system_clock::from_time_t(57600)).businessDate=="19700102","Shanghai business midnight independent of host time zone");
        const auto revoked=create(w);AuthService(path).logout(w.token);
        rejects([&]{svc.submit(w.id,w.token,revoked.id,1,"revoked");},401,"AUTH_INVALID");
        db.execute("UPDATE users SET status='DISABLED' WHERE username='demo_reporter01'");
        rejects([&]{create(r);},401,"AUTH_INVALID");
        check(db.scalarInt("SELECT count(*) FROM pragma_foreign_key_check")==0,"foreign_key_check has no violations");
        auto integrity=db.prepare("PRAGMA integrity_check");check(integrity.step()&&integrity.columnText(0)=="ok","integrity_check ok");
        }
        std::filesystem::remove_all(root);
        std::cout<<"PASS "<<checks<<" work order service/SQLite checks (not HTTP)\n";return 0;
    } catch(const std::exception &e) {std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
}
