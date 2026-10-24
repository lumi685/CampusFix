#include "ReviewTestData.h"
#include "domain/WorkOrderValidation.h"
#include <atomic>
#include <future>
#include <iostream>
#include <limits>
#include <thread>

using namespace campusfix;
namespace {
int checks=0;
void check(bool ok,const std::string &message) {if(!ok) throw std::runtime_error(message);++checks;std::cout<<"PASS SQLite/Service "<<message<<'\n';}
}
int main() {
    const auto root=std::filesystem::temp_directory_path()/("campusfix-review-service-"+newOpaqueId());
    std::filesystem::create_directories(root);
    try {
        const auto path=root/"review.sqlite3";
        const auto f=test::readFixture(path,CAMPUSFIX_MIGRATIONS_DIR,CAMPUSFIX_SEED_FILE);
        {
            SqliteConnection db(path);WorkOrderService service(db);
            const auto dispatcher=f.accounts.at("demo_dispatcher"),admin=f.accounts.at("demo_sysadmin");
            const auto act=[&](const WorkOrder &o,const ReviewInput &i,const test::ReadAccount &a,const std::string &rid="review-service-request") {
                return service.review(a.id,a.token,o.id,i,rid);
            };
            const auto denied=[&](const WorkOrder &o,const ReviewInput &i,const test::ReadAccount &a,int status,const std::string &code) {
                const auto before=test::reviewFingerprint(db);bool caught=false;
                try {act(o,i,a);}catch(const WorkOrderError &e){caught=e.status==status&&e.code==code;}
                check(caught,"rejected "+std::to_string(status)+" "+code);
                check(before==test::reviewFingerprint(db),"failure preserves all business columns and logs");
            };
            const auto successful=[&](const std::string &action,const test::ReadAccount &a,const std::string &owner="demo_reporter01") {
                const auto o=test::pending(db,f,owner);
                const auto logs=db.scalarInt("SELECT count(*) FROM status_logs");
                const auto assignments=db.scalarInt("SELECT count(*) FROM assignments"),dispatch=db.scalarInt("SELECT count(*) FROM dispatch_records");
                const auto input=action=="APPROVE"?test::approve():test::outcome(action);
                const auto r=act(o,input,a,"review-success-"+action);
                const auto target=action=="APPROVE"?"PENDING_DISPATCH":action=="RETURN"?"PENDING_SUPPLEMENT":"CANCELLED";
                check(r.status==target&&r.version==3&&r.action==action,"actual state-machine outcome "+action);
                auto q=db.prepare("SELECT category_id,requested_urgency,approved_urgency,order_no,submitted_at,cancelled_at,updated_at,title,description FROM work_orders WHERE id=?1");q.bindInt(1,o.id);q.step();
                check(q.columnInt(0)==(action=="APPROVE"?2:1)&&q.columnText(1)=="NORMAL"&&
                    (action=="APPROVE"?q.columnText(2)=="HIGH":q.columnIsNull(2)),"classification and separate requested/approved urgency "+action);
                check(q.columnText(3)==*o.orderNo&&q.columnText(4)==*o.submittedAt&&q.columnText(7)==o.input.title&&q.columnText(8)==o.input.description,"original fields and first numbering preserved "+action);
                check(validUtcDateTime(q.columnText(6))&&(action=="REJECT"?(validUtcDateTime(q.columnText(5))&&q.columnText(5)==q.columnText(6)):q.columnIsNull(5)),"UTC cancellation/update semantics "+action);
                check(db.scalarInt("SELECT count(*) FROM status_logs")==logs+1&&db.scalarInt("SELECT count(*) FROM assignments")==assignments&&db.scalarInt("SELECT count(*) FROM dispatch_records")==dispatch,"exactly one event and no dispatch "+action);
                auto log=db.prepare("SELECT actor_id,action,from_status,to_status,public_note,internal_note,metadata_json,created_at,request_id FROM status_logs WHERE order_id=?1 ORDER BY id DESC LIMIT 1");log.bindInt(1,o.id);log.step();
                const auto code=action=="APPROVE"?"ORDER_REVIEWED":action=="RETURN"?"ORDER_RETURNED":"ORDER_REJECTED";
                check(log.columnInt(0)==a.id&&log.columnText(1)==code&&log.columnText(2)=="PENDING_REVIEW"&&log.columnText(3)==target&&log.columnText(8)=="review-success-"+action,"audit actor/action/states/request_id "+action);
                check(log.columnText(5)==*input.internalNote&&(action=="APPROVE"?log.columnIsNull(4):log.columnText(4)==*input.reason),"public reason distinct from internal note "+action);
                const auto reporter=f.accounts.at(owner);const auto timeline=service.timeline(reporter.id,reporter.token,o.id,{});
                check(timeline.items.size()==3&&!timeline.items.back().internalNote&&timeline.items.back().managementDetail.empty()&&timeline.items.back().publicNote==input.reason,"reporter/worker owner timeline projection "+action+" "+owner);
                const auto management=service.timeline(dispatcher.id,dispatcher.token,o.id,{});
                const auto &last=management.items.back();
                check(last.internalNote==input.internalNote&&last.managementDetail.size()==(action=="APPROVE"?3u:1u),"authorized management review whitelist "+action);
                denied(o,input,a,409,"ORDER_STATUS_CONFLICT");
                return o;
            };
            const auto approved=successful("APPROVE",dispatcher);
            successful("RETURN",dispatcher);successful("REJECT",dispatcher);successful("RETURN",dispatcher,"demo_worker01");
            const auto pending=test::pending(db,f);
            denied(pending,test::approve(),f.accounts.at("demo_reporter01"),403,"ACTION_FORBIDDEN");
            const auto workerOrder=test::pending(db,f,"demo_worker01");
            denied(workerOrder,test::approve(),f.accounts.at("demo_worker01"),403,"ACTION_FORBIDDEN");
            denied({f.current},test::approve(),f.accounts.at("demo_worker01"),403,"ACTION_FORBIDDEN");
            const auto foreign=test::pending(db,f,"demo_reporter02",13);
            denied(foreign,test::approve(),dispatcher,404,"RESOURCE_NOT_FOUND");
            denied(pending,test::approve(),admin,404,"RESOURCE_NOT_FOUND");
            const auto isolated=f.accounts.at("test_no_grants");
            test::grant(db,"test_no_grants","AREA_SCOPE",1);
            denied(pending,test::approve(),isolated,404,"RESOURCE_NOT_FOUND");
            test::grant(db,"test_no_grants","REVIEW",1);
            const auto reviewOnlyOrder=test::pending(db,f);
            const auto reviewOnly=act(reviewOnlyOrder,test::approve(),isolated);
            check(reviewOnly.version==3,"explicit REVIEW works without BUSINESS_READ");
            bool hidden=false;
            try {service.timeline(isolated.id,isolated.token,reviewOnlyOrder.id,{});}catch(const WorkOrderError &e){hidden=e.status==404;}
            check(hidden,"REVIEW and AREA_SCOPE do not grant BUSINESS_READ or internal history");
            test::grant(db,"demo_sysadmin","BUSINESS_READ",1);
            denied(pending,test::approve(),admin,403,"ACTION_FORBIDDEN");
            test::grant(db,"demo_sysadmin","AREA_SCOPE",1);
            denied(pending,test::approve(),admin,403,"ACTION_FORBIDDEN");
            test::grant(db,"demo_sysadmin","STATS_VIEW",1);
            denied(pending,test::approve(),admin,403,"ACTION_FORBIDDEN");
            test::grant(db,"demo_sysadmin","REVIEW",2);
            denied(pending,test::approve(),admin,403,"ACTION_FORBIDDEN");
            test::revoke(db,"demo_sysadmin","REVIEW");test::grant(db,"demo_sysadmin","REVIEW",3);
            const auto adminOrder=successful("APPROVE",admin);
            const auto adminTime=service.timeline(admin.id,admin.token,adminOrder.id,{});
            check(!adminTime.items.back().internalNote&&adminTime.items.back().managementDetail.empty(),"REVIEW does not grant INTERNAL_NOTE_READ");
            test::revoke(db,"demo_sysadmin","REVIEW");denied(pending,test::approve(),admin,403,"ACTION_FORBIDDEN");
            for(const auto scope:{7,11}) {test::grant(db,"demo_sysadmin","REVIEW",scope);successful("RETURN",admin);test::revoke(db,"demo_sysadmin","REVIEW");}
            test::grant(db,"demo_sysadmin","REVIEW",{});successful("REJECT",admin);
            test::revoke(db,"demo_dispatcher","AREA_SCOPE");denied(pending,test::approve(),dispatcher,404,"RESOURCE_NOT_FOUND");test::grant(db,"demo_dispatcher","AREA_SCOPE",1);
            db.execute("UPDATE users SET status='DISABLED' WHERE username='demo_dispatcher'");denied(pending,test::approve(),dispatcher,401,"AUTH_INVALID");db.execute("UPDATE users SET status='ACTIVE' WHERE username='demo_dispatcher'");
            auto invalidToken=dispatcher;invalidToken.token="invalid-session";denied(pending,test::approve(),invalidToken,401,"AUTH_INVALID");
            for(const auto state:{"DRAFT","PENDING_SUPPLEMENT","PENDING_DISPATCH","PENDING_ACCEPT","IN_PROGRESS","PENDING_VERIFY","COMPLETED","CLOSED","CANCELLED"}) {
                auto q=db.prepare("UPDATE work_orders SET status=?1 WHERE id=?2");q.bindText(1,state);q.bindInt(2,pending.id);q.step();
                for(const auto action:{"APPROVE","RETURN","REJECT"}) denied(pending,action==std::string("APPROVE")?test::approve():test::outcome(action),dispatcher,409,"ORDER_STATUS_CONFLICT");
            }
            {auto q=db.prepare("UPDATE work_orders SET status='PENDING_REVIEW' WHERE id=?1");q.bindInt(1,pending.id);q.step();}
            auto bad=test::approve();bad.expectedVersion=1;denied(pending,bad,dispatcher,409,"ORDER_VERSION_CONFLICT");
            bad=test::approve();bad.action="CANCEL";denied(pending,bad,dispatcher,422,"VALIDATION_ERROR");
            bad=test::approve();bad.categoryId.reset();denied(pending,bad,dispatcher,422,"VALIDATION_ERROR");
            bad=test::approve();bad.approvedUrgency.reset();denied(pending,bad,dispatcher,422,"VALIDATION_ERROR");
            bad=test::approve();bad.approvedUrgency="invalid";denied(pending,bad,dispatcher,422,"VALIDATION_ERROR");
            bad=test::approve();bad.categoryId=999999;denied(pending,bad,dispatcher,422,"VALIDATION_ERROR");
            db.execute("UPDATE fault_categories SET active=0 WHERE id=2");bad=test::approve();denied(pending,bad,dispatcher,422,"VALIDATION_ERROR");db.execute("UPDATE fault_categories SET active=1 WHERE id=2");
            for(const auto action:{"RETURN","REJECT"}) {
                for(const auto &reason:{std::string(),std::string(" \t\r\n"),std::string(501,'x'),std::string("\xC0\x80"),std::string("a\0b",3)}) {
                    bad=test::outcome(action);bad.reason=reason;denied(pending,bad,dispatcher,422,"VALIDATION_ERROR");
                }
                bad=test::outcome(action);bad.reason.reset();denied(pending,bad,dispatcher,422,"VALIDATION_ERROR");
                const auto o=test::pending(db,f);bad=test::outcome(action);bad.reason=std::string(500,'x');act(o,bad,dispatcher);check(true,"500-character reason boundary accepted");
                std::string unicode;for(int i=0;i<500;++i) unicode+=u8"修";
                const auto u=test::pending(db,f);bad.reason=unicode;act(u,bad,dispatcher);check(true,"500 Unicode scalar values accepted, not 500 bytes");
                bad.reason=unicode+u8"修";denied(pending,bad,dispatcher,422,"VALIDATION_ERROR");
            }
            for(const auto *urgency:{"LOW","NORMAL","HIGH","URGENT"}) {
                const auto o=test::pending(db,f);auto input=test::approve();input.approvedUrgency=urgency;input.internalNote.reset();
                check(act(o,input,dispatcher).version==3,"approved urgency enum and omitted internal note "+std::string(urgency));
            }
            {
                const auto o=test::pending(db,f);auto input=test::outcome("RETURN");input.reason=u8"修";input.internalNote="";
                check(act(o,input,dispatcher).version==3,"one Unicode reason and optional empty note accepted");
            }
            for(const auto *action:{"APPROVE","RETURN","REJECT"}) {
                for(const auto length:{1999,2000}) {
                    std::string note;for(int n=0;n<length;++n) note+=u8"修";
                    const auto o=test::pending(db,f);auto input=std::string(action)=="APPROVE"?test::approve():test::outcome(action);input.internalNote=note;
                    act(o,input,dispatcher);
                    const auto page=service.timeline(dispatcher.id,dispatcher.token,o.id,{});
                    check(page.items.back().internalNote==note,"internal note Unicode boundary "+std::to_string(length)+" "+action);
                }
                for(const auto &note:{std::string(2001,'x'),std::string(2001,' '),std::string("\xC0\x80"),std::string("a\0b",3)}) {
                    auto input=std::string(action)=="APPROVE"?test::approve():test::outcome(action);input.internalNote=note;
                    denied(pending,input,dispatcher,422,"VALIDATION_ERROR");
                }
                for(const auto &note:std::vector<std::optional<std::string>>{{},std::string(),std::string(" \t\r\n"),std::string(u8"　 \t")}) {
                    const auto o=test::pending(db,f);auto input=std::string(action)=="APPROVE"?test::approve():test::outcome(action);input.internalNote=note;
                    act(o,input,dispatcher);
                    const auto page=service.timeline(dispatcher.id,dispatcher.token,o.id,{});
                    check(!page.items.back().internalNote&&page.items.back().publicNote==input.reason,"omitted/empty/blank note stored as NULL without changing public reason "+std::string(action));
                }
            }
            {
                std::string note;for(int n=0;n<2000;++n) note+="\xF0\x9F\x94\xA7";
                auto input=test::approve();input.internalNote=note;const auto o=test::pending(db,f);
                check(act(o,input,dispatcher).version==3,"2000 supplementary Unicode scalars accepted (8000 UTF-8 bytes)");
                input.internalNote=note+"\xF0\x9F\x94\xA7";denied(pending,input,dispatcher,422,"VALIDATION_ERROR");
            }
            for(const auto trigger:{
                "CREATE TRIGGER fail_review BEFORE INSERT ON status_logs WHEN NEW.action IN ('ORDER_REVIEWED','ORDER_RETURNED','ORDER_REJECTED') BEGIN SELECT RAISE(ABORT,'fixture log failure'); END",
                "CREATE TRIGGER fail_review BEFORE UPDATE OF category_id,approved_urgency ON work_orders BEGIN SELECT RAISE(ABORT,'fixture approval failure'); END",
                "CREATE TRIGGER fail_review AFTER UPDATE OF cancelled_at ON work_orders BEGIN SELECT RAISE(ABORT,'fixture cancellation failure'); END"}) {
                db.execute(trigger);
                const bool logFailure=std::string(trigger).find("INSERT ON status_logs")!=std::string::npos;
                for(const auto *a:{"APPROVE","RETURN","REJECT"}) {
                    if(!logFailure&&((std::string(trigger).find("cancelled_at")!=std::string::npos)!=(std::string(a)=="REJECT"))) continue;
                    if(!logFailure&&std::string(a)=="RETURN") continue;
                    const auto input=std::string(a)=="APPROVE"?test::approve():test::outcome(a);
                    const auto before=test::reviewFingerprint(db);bool failed=false;
                    try {act(pending,input,dispatcher);}catch(const SqliteError &){failed=true;}
                    check(failed&&before==test::reviewFingerprint(db),"injected failure rolls back every business column/log "+std::string(a));
                }
                db.execute("DROP TRIGGER fail_review");
            }
            db.execute("CREATE TRIGGER ignore_review BEFORE UPDATE OF approved_urgency ON work_orders BEGIN SELECT RAISE(IGNORE); END");
            denied(pending,test::approve(),dispatcher,409,"ORDER_VERSION_CONFLICT");
            db.execute("DROP TRIGGER ignore_review");
            {
                const auto o=test::pending(db,f);auto input=test::approve();input.expectedVersion=std::numeric_limits<std::int64_t>::max();
                auto q=db.prepare("UPDATE work_orders SET version=?1 WHERE id=?2");q.bindInt(1,input.expectedVersion);q.bindInt(2,o.id);q.step();
                denied(o,input,dispatcher,409,"ORDER_VERSION_CONFLICT");
            }
            // Both write connections rendezvous before entering BEGIN IMMEDIATE.
            const auto concurrent=[&](bool different) {
                const auto o=test::pending(db,f);const auto logCount=db.scalarInt("SELECT count(*) FROM status_logs");std::atomic<int> ready{0};
                const auto run=[&](bool second) {
                    SqliteConnection other(path);++ready;while(ready.load()<2) std::this_thread::yield();
                    const auto a=second?admin:dispatcher;const auto i=second&&different?test::outcome("RETURN"):test::approve();
                    try {WorkOrderService(other).review(a.id,a.token,o.id,i,second?"race-b":"race-a");return 200;}
                    catch(const WorkOrderError &e){return e.status;}
                };
                auto first=std::async(std::launch::async,run,false),second=std::async(std::launch::async,run,true);
                const auto a=first.get(),b=second.get();
                const auto final=WorkOrderRepository(db).find(o.id);
                check((a==200&&b==409)||(a==409&&b==200),different?"concurrent APPROVE/RETURN one success one 409":"concurrent APPROVE/APPROVE one success one 409");
                check(final.version==3&&db.scalarInt("SELECT count(*) FROM status_logs")==logCount+1,"concurrent winner increments once and appends once");
            };
            concurrent(false);concurrent(true);
            test::integrity(db);check(true,"integrity_check=ok; foreign_key_check empty");
            (void)approved;
        }
        std::filesystem::remove_all(root);
        std::cout<<"PASS "<<checks<<" SQLite/Service assertions; 0 new pure Policy tests\n";return 0;
    }catch(const std::exception &e){std::cerr<<e.what()<<'\n';std::filesystem::remove_all(root);return 1;}
}
