#include "WorkerTaskTestData.h"
#include "services/CompletionService.h"
#include "services/TransferService.h"
#include "services/ProgressEventService.h"
#include "domain/WorkOrderValidation.h"
#include <atomic>
#include <future>
#include <iostream>
#include <limits>
#include <thread>
using namespace campusfix;
namespace {
int cases=0,checks=0;
void check(bool b,const char *m){if(!b)throw std::runtime_error(m);++checks;}
void scenario(const char *s){++cases;std::cout<<"CASE "<<s<<'\n';}
template<class F>void denied(SqliteConnection &db,int status,F fn){const auto before=test::dispatchFingerprint(db);bool caught=false;try{fn();}catch(const WorkOrderError &e){caught=e.status==status;}check(caught,"expected rejection");check(test::dispatchFingerprint(db)==before,"rejection unchanged all16 tables");}
CompletionResult complete(SqliteConnection &db,const test::ReadAccount &w,std::int64_t id,std::int64_t version=5,const std::string &summary="Fictional repair completed") {return CompletionService(db).complete(w.id,w.token,id,version,summary,"test-completion-request");}
void set(SqliteConnection &db,const char *sql,std::int64_t id){auto q=db.prepare(sql);q.bindInt(1,id);q.step();}
void services(const std::filesystem::path &path,const test::ReadData &f){SqliteConnection db(path);const auto w=f.accounts.at("demo_worker01"),admin=f.accounts.at("demo_dispatcher");
    scenario("T19 result, assignment time/version, full audit, no release or history mutation");
    const auto a=test::taskOrder(db,f);test::taskAction(db,w,a.orderId);const auto before=test::inspectDispatch(db,a.orderId);const auto summary=std::string(u8"已完成虚构维修。\n保留完整说明 😀");
    const auto r=complete(db,w,a.orderId,5,summary);const auto after=test::inspectDispatch(db,a.orderId);
    check(r.orderId==a.orderId&&r.version==6&&r.status=="PENDING_VERIFY"&&r.summary==summary,"exact result DTO");
    auto q=db.prepare("SELECT repair_summary,rework_count,updated_at,completed_at FROM work_orders WHERE id=?1");q.bindInt(1,a.orderId);q.step();check(q.columnText(0)==summary&&q.columnInt(1)==0&&q.columnIsNull(3),"summary saved not verified/reworked");
    const auto &assignment=after["assignments"][0];check(assignment["ended_at"].isNull()&&assignment["end_reason"].isNull()&&assignment["version"]=="3","assignment remains live version increments");
    check(assignment["accepted_at"]==before["assignments"][0]["accepted_at"]&&validUtcDateTime(assignment["result_submitted_at"].asString()),"accept unchanged latest result UTC");
    check(after["worker_loads"]==before["worker_loads"]&&after["dispatch_records"]==before["dispatch_records"],"capacity and dispatch snapshot retained");
    check(after["status_logs"].size()==before["status_logs"].size()+1,"exactly one appended event");const auto log=after["status_logs"][after["status_logs"].size()-1];
    check(log["action"]=="ORDER_COMPLETED"&&log["public_note"]==summary&&log["assignment_id"]==std::to_string(a.assignmentId)&&log["actor_id"]==std::to_string(w.id)&&log["request_id"]=="test-completion-request","full immutable audit snapshot");
    check(log["from_status"]=="IN_PROGRESS"&&log["to_status"]=="PENDING_VERIFY"&&log["created_at"]==assignment["result_submitted_at"]&&q.columnText(2)==log["created_at"].asString(),"one UTC across writes");
    const auto reporter=f.accounts.at("demo_reporter01");const auto timeline=WorkOrderService(db).timeline(reporter.id,reporter.token,a.orderId,{});check(timeline.items.back().publicNote==summary,"reporter reads full result");
    denied(db,404,[&]{const auto x=f.accounts.at("demo_reporter02");WorkOrderService(db).timeline(x.id,x.token,a.orderId,{});});denied(db,409,[&]{complete(db,w,a.orderId);});
    scenario("pending transfers superseded only for this live assignment, mine public history remains");
    const auto b=test::taskOrder(db,f),other=test::taskOrder(db,f);test::taskAction(db,w,b.orderId);const auto bt=TransferService(db).create(w.id,w.token,b.orderId,5,"Fictional transfer","test-transfer");test::pendingTransfer(db,other,w.id);
    complete(db,w,b.orderId);auto t=db.prepare("SELECT status,reviewed_by,review_reason,resolved_at FROM transfer_requests WHERE id=?1");t.bindInt(1,bt.id);t.step();check(t.columnText(0)=="SUPERSEDED"&&t.columnIsNull(1)&&!t.columnText(2).empty()&&validUtcDateTime(t.columnText(3)),"system resolution not manual approval");check(TransferRepository(db).pending(other.assignmentId).has_value(),"other application unaffected");
    denied(db,409,[&]{TransferService(db).decide(admin.id,admin.token,b.orderId,bt.id,6,"APPROVE","Too late","test-decision");});
    scenario("text Unicode boundaries including 2000 Chinese and malformed UTF8");
    const auto c=test::taskOrder(db,f);test::taskAction(db,w,c.orderId);
    for(const auto &text:std::vector<std::string>{"",u8"\u3000\u00a0\uFEFF",std::string(2001,'x'),std::string("\xC0\x80"),std::string("\xED\xA0\x80"),std::string("\xF0\x9F"),std::string(1,'\0'),std::string(1,'\x01')})denied(db,422,[&]{complete(db,w,c.orderId,5,text);});
    std::string chinese;for(int i=0;i<2000;i++)chinese+=u8"修";check(complete(db,w,c.orderId,5,chinese).summary==chinese,"2000 Unicode not byte length");
    scenario("role, current ownership, valid session and state/version authoritative");
    const auto d=test::taskOrder(db,f);denied(db,409,[&]{complete(db,w,d.orderId,4);});test::taskAction(db,w,d.orderId);
    for(const auto *name:{"demo_reporter01","demo_worker02","demo_dispatcher","demo_sysadmin"})denied(db,std::string(name)=="demo_worker02"||std::string(name)=="demo_sysadmin"?404:403,[&]{complete(db,f.accounts.at(name),d.orderId);});
    denied(db,409,[&]{complete(db,w,d.orderId,4);});auto bad=w;bad.token=newToken();denied(db,401,[&]{complete(db,bad,d.orderId);});
    set(db,"UPDATE users SET status='DISABLED' WHERE id=?1",w.id);denied(db,401,[&]{complete(db,w,d.orderId);});set(db,"UPDATE users SET status='ACTIVE' WHERE id=?1",w.id);
    for(const auto *state:{"PENDING_VERIFY","COMPLETED","CLOSED","CANCELLED"}) {auto update=db.prepare("UPDATE work_orders SET status=?1 WHERE id=?2");update.bindText(1,state);update.bindInt(2,d.orderId);update.step();denied(db,409,[&]{complete(db,w,d.orderId);});}
    const auto historical=test::taskOrder(db,f);test::taskAction(db,w,historical.orderId);set(db,"UPDATE assignments SET ended_at='2026-11-01T00:00:00.000Z',end_reason='TRANSFERRED' WHERE order_id=?1",historical.orderId);denied(db,403,[&]{complete(db,w,historical.orderId);});
    scenario("missing accept, overflow and invalid future timestamps roll back");
    for(int m=0;m<6;m++){const auto x=test::taskOrder(db,f);test::taskAction(db,w,x.orderId);const char *sql=m==0?"UPDATE assignments SET accepted_at=NULL WHERE order_id=?1":m==1?"UPDATE work_orders SET version=9223372036854775807 WHERE id=?1":m==2?"UPDATE assignments SET version=9223372036854775807 WHERE order_id=?1":m==3?"UPDATE assignments SET accepted_at='2099-01-01T00:00:00.000Z' WHERE order_id=?1":m==4?"UPDATE assignments SET result_submitted_at='2099-01-01T00:00:00.000Z' WHERE order_id=?1":"UPDATE assignments SET end_reason='COMPLETED' WHERE order_id=?1";set(db,sql,x.orderId);denied(db,409,[&]{complete(db,w,x.orderId,m==1?std::numeric_limits<std::int64_t>::max():5);});}
    scenario("all four SQL stages abort/ignore roll back, final account revocation rolls back");
    const auto x=test::taskOrder(db,f);test::taskAction(db,w,x.orderId);TransferService(db).create(w.id,w.token,x.orderId,5,"Pending during fault","test-transfer");
    for(const auto *table:{"work_orders","assignments","transfer_requests","status_logs"})for(const bool ignore:{false,true}){
        // Ignoring an append is not a DB error; repository append must check changes.
        const std::string action=std::string(table)=="status_logs"?"INSERT":"UPDATE";const auto fingerprint=test::dispatchFingerprint(db);
        db.execute("CREATE TRIGGER completion_failure BEFORE "+action+" ON "+table+" BEGIN SELECT RAISE("+(ignore?std::string("IGNORE"):std::string("ABORT,'PRIVATE_COMPLETION_FAILURE'"))+"); END");
        bool failed=false;try{complete(db,w,x.orderId);}catch(const std::exception &){failed=true;}
        check(failed,"fault or ignored mutation detected");check(test::dispatchFingerprint(db)==fingerprint,"all16 tables rolled back");db.execute("DROP TRIGGER completion_failure");
    }
    db.execute("CREATE TRIGGER completion_revoke AFTER INSERT ON status_logs WHEN NEW.action='ORDER_COMPLETED' BEGIN UPDATE users SET status='DISABLED' WHERE id=NEW.actor_id; END");denied(db,401,[&]{complete(db,w,x.orderId);});db.execute("DROP TRIGGER completion_revoke");
    scenario("isolated rework fixture updates final time, immutable earlier log, not an API24 test");
    const auto re=test::taskOrder(db,f);test::taskAction(db,w,re.orderId);complete(db,w,re.orderId,5,"First fictional result");const auto initial=test::inspectDispatch(db,re.orderId);const auto firstLog=initial["status_logs"][initial["status_logs"].size()-1];
    set(db,"UPDATE work_orders SET status='IN_PROGRESS',version=version+1,rework_count=1 WHERE id=?1",re.orderId);
    const auto future=std::chrono::system_clock::now()+std::chrono::seconds(2);const auto last=CompletionService(db,[future]{return future;}).complete(w.id,w.token,re.orderId,7,"Second fictional result","second-result-request");const auto final=test::inspectDispatch(db,re.orderId);
    check(last.version==8&&final["status_logs"][initial["status_logs"].size()-1]==firstLog,"first result/time unmodified");check(final["assignments"][0]["result_submitted_at"]!=initial["assignments"][0]["result_submitted_at"],"latest result time replaced");
    check(final["worker_loads"]==initial["worker_loads"]&&final["dispatch_records"]==initial["dispatch_records"],"rework also retains capacity/history");
    auto rw=db.prepare("SELECT rework_count,repair_summary FROM work_orders WHERE id=?1");rw.bindInt(1,re.orderId);rw.step();check(rw.columnInt(0)==1&&rw.columnText(1)=="Second fictional result","complete does not increment rework count");
    test::integrity(db);check(true,"integrity ok FK empty");
}
int trace(unsigned,void *c,void *s,void *){const auto *sql=sqlite3_sql(static_cast<sqlite3_stmt *>(s));if(sql&&std::string(sql)=="BEGIN IMMEDIATE")++*static_cast<std::atomic<int> *>(c);return 0;}
void races(const std::filesystem::path &path,const test::ReadData &f){SqliteConnection db(path);const auto w=f.accounts.at("demo_worker01"),admin=f.accounts.at("demo_dispatcher");
    for(int mode=0;mode<5;mode++){scenario("two real BEGIN attempts with independent third connection lock");const auto x=test::taskOrder(db,f);test::taskAction(db,w,x.orderId);std::int64_t request=0;if(mode==1||mode==4)request=TransferService(db).create(w.id,w.token,x.orderId,5,"Race transfer","race-transfer").id;
        std::atomic<int> attempts{0};SqliteTransaction holder(db);const auto run=[&](bool second){SqliteConnection other(path);sqlite3_trace_v2(other.nativeHandle(),SQLITE_TRACE_STMT,trace,&attempts);try{
            if(!second||mode==0){complete(other,w,x.orderId);return 200;}
            if(mode==1||mode==4){TransferService(other).decide(admin.id,admin.token,x.orderId,request,5,mode==1?"APPROVE":"REJECT","Race decision","race-decision");return 200;}
            if(mode==2){ProgressEventService(other).append(w.id,w.token,x.orderId,5,"PROGRESS_NOTE","Race progress","race-progress");return 201;}
            TransferService(other).create(w.id,w.token,x.orderId,5,"Race transfer","race-transfer");return 201;
        }catch(const WorkOrderError &e){return e.status;}};
        auto one=std::async(std::launch::async,run,false),two=std::async(std::launch::async,run,true);const auto end=std::chrono::steady_clock::now()+std::chrono::seconds(3);while(attempts<2&&std::chrono::steady_clock::now()<end)std::this_thread::sleep_for(std::chrono::milliseconds(1));check(attempts==2,"both BEGIN attempted before releasing blocker");holder.commit();const auto a=one.get(),b=two.get();
        if(mode==0)check((a==200&&b==409)||(a==409&&b==200),"one completion only");
        else if(mode==1)check((a==200&&b==409)||(a==403&&b==200),"completion or approved transfer only");
        else if(mode==4)check(a==200&&(b==200||b==409),"reject may serialize before complete without state/version change");
        else check(a==200&&(b==201||b==409),"progress/application before complete or conflict after");
        const auto s=test::inspectDispatch(db,x.orderId);check(s["dispatch_records"].size()==1&&s["assignments"].size()==1,"history retained");
        int completions=0;for(const auto &log:s["status_logs"])if(log["action"]=="ORDER_COMPLETED")completions++;check(completions==((mode==0||a==200)?1:0),"one real completion log per winning submission");std::cout<<"RACE mode="<<mode<<" statuses="<<a<<','<<b<<'\n';
    }test::integrity(db);
}
}
int main(int argc,char **argv){initializeCrypto();const auto root=std::filesystem::temp_directory_path()/("campusfix-completion-"+newOpaqueId());std::filesystem::create_directories(root);int code=0;try{const auto path=root/"test.sqlite3";const auto f=test::taskFixture(path,CAMPUSFIX_MIGRATIONS_DIR,CAMPUSFIX_SEED_FILE);if(argc>1&&std::string(argv[1])=="--races")races(path,f);else services(path,f);std::cout<<"PASS completion cases="<<cases<<" assertions="<<checks<<'\n';}catch(const std::exception &e){std::cerr<<"FAIL completion "<<e.what()<<'\n';code=1;}std::error_code ec;std::filesystem::remove_all(root,ec);return ec?1:code;}
