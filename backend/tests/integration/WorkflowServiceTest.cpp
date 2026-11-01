#include "WorkerTaskTestData.h"
#include "services/TransferService.h"
#include "services/TransferQueryService.h"
#include "services/ProgressEventService.h"
#include "services/WorkOrderService.h"
#include "domain/WorkOrderValidation.h"
#include <atomic>
#include <future>
#include <iostream>
#include <limits>
#include <thread>
using namespace campusfix;
namespace {
int cases=0,checks=0;
void check(bool value,const char *message){if(!value)throw std::runtime_error(message);++checks;}
void scenario(const char *name){++cases;std::cout<<"CASE "<<name<<'\n';}
template<class F>void denied(SqliteConnection &db,int status,F fn){const auto before=test::dispatchFingerprint(db);bool rejected=false;try{fn();}catch(const WorkOrderError &e){rejected=e.status==status;}check(rejected,"expected safe rejection");check(before==test::dispatchFingerprint(db),"rejection leaves all16 tables unchanged");}
TransferCreated apply(SqliteConnection &db,const test::ReadAccount &a,std::int64_t id,std::int64_t version=4,const std::string &reason="Fictional specialist needed"){return TransferService(db).create(a.id,a.token,id,version,reason,"test-transfer-apply");}
TransferDecision decide(SqliteConnection &db,const test::ReadAccount &a,std::int64_t id,std::int64_t request,std::int64_t version=4,const std::string &decision="APPROVE",const std::string &reason="Fictional management decision"){return TransferService(db).decide(a.id,a.token,id,request,version,decision,reason,"test-transfer-decision");}
ProgressEvent event(SqliteConnection &db,const test::ReadAccount &a,std::int64_t id,const std::string &type="ARRIVED",std::optional<std::string> note={},std::int64_t version=5){return ProgressEventService(db).append(a.id,a.token,id,version,type,note,"test-progress-event");}
void services(const std::filesystem::path &path,const test::ReadData &f){SqliteConnection db(path);const auto w=f.accounts.at("demo_worker01"),other=f.accounts.at("demo_worker02"),admin=f.accounts.at("demo_dispatcher"),system=f.accounts.at("demo_sysadmin");
    scenario("pending accept application no state/version/load change, and real accept remains allowed");
    const auto a=test::taskOrder(db,f);const auto load=test::activeTaskCount(db);const auto t=apply(db,w,a.orderId);auto s=test::inspectDispatch(db,a.orderId);
    check(t.id>0&&t.orderStatus=="PENDING_ACCEPT"&&s["version"]==4,"created response and version stable");check(s["assignments"][0]["ended_at"].isNull()&&s["assignments"][0]["version"]=="1"&&test::activeTaskCount(db)==load,"application no release or assignment mutation");check(s["status_logs"].size()==5&&s["status_logs"][4]["assignment_id"]==std::to_string(a.assignmentId),"application audit true assignment");
    denied(db,409,[&]{apply(db,w,a.orderId);});const auto accepted=test::taskAction(db,w,a.orderId);check(accepted.version==5,"accept after application");check(TransferRepository(db).pending(a.assignmentId)==t.id,"application retained after accept");
    {SqliteConnection read(path);const auto p=TransferQueryService(read).list(w.id,w.token,{"mine",1,20,a.orderId});check(p.items[0].orderVersion==5,"query current version not application snapshot");}
    scenario("reject changes only request and log, then reapply/approve preserves history and releases capacity");
    const auto rejected=decide(db,admin,a.orderId,t.id,5,"REJECT");check(!rejected.assignmentEnded&&rejected.version==5&&rejected.orderStatus=="IN_PROGRESS","reject not refusal");check(test::activeTaskCount(db)==load,"reject keeps capacity");denied(db,409,[&]{decide(db,admin,a.orderId,t.id,5);});
    const auto next=apply(db,w,a.orderId,5);check(next.id!=t.id,"new application after rejection");const auto approved=decide(db,admin,a.orderId,next.id,5);s=test::inspectDispatch(db,a.orderId);
    check(approved.assignmentEnded&&approved.version==6&&s["status"]=="PENDING_DISPATCH","approve state/version");check(s["assignments"].size()==1&&s["dispatch_records"].size()==1&&s["assignments"][0]["end_reason"]=="TRANSFERRED"&&s["assignments"][0]["version"]=="3","original accepted assignment/snapshot retained");check(test::activeTaskCount(db)==load-1,"approval releases exactly one");check(s["status_logs"][s["status_logs"].size()-1]["assignment_id"]==std::to_string(a.assignmentId),"approval assignment audit");
    denied(db,403,[&]{event(db,w,a.orderId);});denied(db,403,[&]{apply(db,w,a.orderId,6);});check(DispatchService(db).recommend(admin.id,admin.token,a.orderId)["top_candidates"].size()>0,"recommendation legal again, no auto assign");
    scenario("approve pending-accept request without accepting, no new assignment");
    const auto b=test::taskOrder(db,f);const auto bt=apply(db,w,b.orderId);const auto beforeLoad=test::activeTaskCount(db);decide(db,admin,b.orderId,bt.id);s=test::inspectDispatch(db,b.orderId);check(s["version"]==5&&s["assignments"][0]["accepted_at"].isNull(),"T17 not implicit accept");check(test::activeTaskCount(db)==beforeLoad-1,"T17 releases capacity");
    scenario("request ID/order/assignment/state/version security and no administrator-initiated transfer");
    const auto c=test::taskOrder(db,f),d=test::taskOrder(db,f);const auto ct=apply(db,w,c.orderId);denied(db,404,[&]{decide(db,admin,d.orderId,ct.id);});denied(db,404,[&]{decide(db,admin,d.orderId,999999);});denied(db,409,[&]{decide(db,admin,c.orderId,ct.id,3);});
    denied(db,404,[&]{apply(db,other,c.orderId);});denied(db,403,[&]{apply(db,admin,c.orderId);});denied(db,403,[&]{apply(db,f.accounts.at("demo_reporter01"),c.orderId);});denied(db,404,[&]{decide(db,system,c.orderId,ct.id);});
    test::grant(db,"demo_sysadmin","BUSINESS_READ",1);denied(db,403,[&]{decide(db,system,c.orderId,ct.id);});test::grant(db,"demo_sysadmin","DISPATCH",2);denied(db,403,[&]{decide(db,system,c.orderId,ct.id);});test::revoke(db,"demo_sysadmin","DISPATCH");test::grant(db,"demo_sysadmin","DISPATCH",1);decide(db,system,c.orderId,ct.id,4,"REJECT");
    db.execute("UPDATE assignments SET ended_at='2026-11-01T00:00:00.000Z',end_reason='TRANSFERRED' WHERE id="+std::to_string(d.assignmentId));denied(db,403,[&]{apply(db,w,d.orderId);});
    for(const auto *status:{"PENDING_VERIFY","COMPLETED"}) {auto q=db.prepare("UPDATE work_orders SET status=?1 WHERE id=?2");q.bindText(1,status);q.bindInt(2,c.orderId);q.step();denied(db,409,[&]{apply(db,w,c.orderId);});}
    scenario("Unicode boundaries, decision validation and safe authentication");
    const auto e=test::taskOrder(db,f);for(const auto &reason:std::vector<std::string>{"",u8"\u3000\u00a0\u2009",std::string(501,'x'),std::string("\xC0\x80"),std::string(1,'\0')})denied(db,422,[&]{apply(db,w,e.orderId,4,reason);});
    const auto et=apply(db,w,e.orderId);denied(db,422,[&]{decide(db,admin,e.orderId,et.id,4,"OTHER");});denied(db,422,[&]{decide(db,admin,e.orderId,et.id,4,"REJECT","");});
    auto bad=w;bad.token=newToken();denied(db,401,[&]{apply(db,bad,e.orderId);});db.execute("UPDATE users SET status='DISABLED' WHERE username='demo_worker01'");denied(db,401,[&]{apply(db,w,e.orderId);});db.execute("UPDATE users SET status='ACTIVE' WHERE username='demo_worker01'");
    scenario("arrival and progress change only append-only log, current version and load unchanged");
    const auto p=test::taskOrder(db,f);denied(db,409,[&]{event(db,w,p.orderId,"ARRIVED",{},4);});test::taskAction(db,w,p.orderId);const auto initial=test::inspectDispatch(db,p.orderId);const auto arrived=event(db,w,p.orderId);
    check(arrived.orderVersion==5&&arrived.assignmentId==p.assignmentId&&validUtcDateTime(arrived.createdAt),"real arrival DTO");denied(db,409,[&]{event(db,w,p.orderId);});denied(db,422,[&]{event(db,w,p.orderId,"PROGRESS_NOTE");});denied(db,422,[&]{event(db,w,p.orderId,"PROGRESS_NOTE",std::string(1001,'x'));});denied(db,422,[&]{event(db,w,p.orderId,"ARRIVED",std::string(501,'x'));});
    const auto first=event(db,w,p.orderId,"PROGRESS_NOTE","Fictional progress one"),second=event(db,w,p.orderId,"PROGRESS_NOTE",std::string(1000,'x'));check(arrived.id<first.id&&first.id<second.id,"real log sequence");s=test::inspectDispatch(db,p.orderId);check(s["status"]=="IN_PROGRESS"&&s["version"]==5&&s["assignments"]==initial["assignments"]&&s["worker_loads"]==initial["worker_loads"]&&s["dispatch_records"]==initial["dispatch_records"],"only logs changed, no capacity/efficiency timestamps");
    const auto timeline=WorkOrderService(db).timeline(w.id,w.token,p.orderId,{});check(timeline.items[timeline.items.size()-3].action=="WORKER_ARRIVED"&&timeline.items.back().action=="WORKER_PROGRESS_NOTE","public timeline actual order");denied(db,404,[&]{event(db,other,p.orderId);});denied(db,409,[&]{event(db,w,p.orderId,"PROGRESS_NOTE","Stale",4);});
    scenario("actual partial unique indexes reject duplicate pending and arrival rows");
    bool duplicate=false;try{test::pendingTransfer(db,e,w.id);}catch(const SqliteError &x){duplicate=x.primaryCode()==SQLITE_CONSTRAINT;}check(duplicate,"real pending unique index");
    duplicate=false;try{auto q=db.prepare("INSERT INTO status_logs(order_id,actor_id,assignment_id,action,from_status,to_status,created_at,request_id) VALUES(?1,?2,?3,'WORKER_ARRIVED','IN_PROGRESS','IN_PROGRESS','2026-11-01T00:00:00.000Z','unique-index-test')");q.bindInt(1,p.orderId);q.bindInt(2,w.id);q.bindInt(3,p.assignmentId);q.step();}catch(const SqliteError &x){duplicate=x.primaryCode()==SQLITE_CONSTRAINT;}check(duplicate,"real arrived unique index");
    scenario("approval version overflow cannot partially end assignment");
    for(const bool assignment:{false,true}){const auto x=test::taskOrder(db,f);const auto xt=apply(db,w,x.orderId);auto q=db.prepare(assignment?"UPDATE assignments SET version=?1 WHERE id=?2":"UPDATE work_orders SET version=?1 WHERE id=?2");q.bindInt(1,std::numeric_limits<std::int64_t>::max());q.bindInt(2,assignment?x.assignmentId:x.orderId);q.step();denied(db,409,[&]{decide(db,admin,x.orderId,xt.id,assignment?4:std::numeric_limits<std::int64_t>::max());});}
    scenario("all workflow SQL stages roll back request, assignment, order and audit atomically");
    for(const auto *operation:{"create","approve","reject","arrived","progress"}){
        const std::string op=operation;const auto x=test::taskOrder(db,f);std::int64_t request=0;if(op=="approve"||op=="reject")request=apply(db,w,x.orderId).id;if(op=="arrived"||op=="progress")test::taskAction(db,w,x.orderId);
        const std::vector<std::string> tables=op=="create"?std::vector<std::string>{"transfer_requests","status_logs"}:op=="approve"?std::vector<std::string>{"transfer_requests","assignments","work_orders","status_logs"}:op=="reject"?std::vector<std::string>{"transfer_requests","status_logs"}:std::vector<std::string>{"status_logs"};
        for(const auto &table:tables){const auto before=test::dispatchFingerprint(db);const auto action=op=="create"||table=="status_logs"?"INSERT":"UPDATE";db.execute("CREATE TRIGGER workflow_failure BEFORE "+std::string(action)+" ON "+table+" BEGIN SELECT RAISE(ABORT,'PRIVATE_WORKFLOW_FAILURE'); END");bool failed=false;
            try{if(op=="create")apply(db,w,x.orderId);else if(op=="approve"||op=="reject")decide(db,admin,x.orderId,request,4,op=="approve"?"APPROVE":"REJECT");else event(db,w,x.orderId,op=="arrived"?"ARRIVED":"PROGRESS_NOTE","Fictional fault note");}catch(const SqliteError &){failed=true;}
            check(failed,"SQL fault actually reached");check(before==test::dispatchFingerprint(db),"all16 tables rolled back");db.execute("DROP TRIGGER workflow_failure");}
    }
    scenario("in-transaction authorization revocation fails closed and rolls back every business row");
    const auto protectedOrder=test::taskOrder(db,f);const auto protectedRequest=apply(db,w,protectedOrder.orderId);const auto fingerprint=test::dispatchFingerprint(db);
    db.execute("CREATE TRIGGER workflow_revoke AFTER UPDATE ON transfer_requests BEGIN UPDATE management_grants SET revoked_at='2026-11-01T00:00:00.000Z' WHERE capability='DISPATCH'; END");
    denied(db,403,[&]{decide(db,system,protectedOrder.orderId,protectedRequest.id);});check(fingerprint==test::dispatchFingerprint(db),"revoked grant and partial workflow updates all rolled back");db.execute("DROP TRIGGER workflow_revoke");
    test::integrity(db);check(true,"integrity ok FK empty");
}
int trace(unsigned,void *c,void *s,void *){const auto *sql=sqlite3_sql(static_cast<sqlite3_stmt *>(s));if(sql&&std::string(sql)=="BEGIN IMMEDIATE")++*static_cast<std::atomic<int> *>(c);return 0;}
void races(const std::filesystem::path &path,const test::ReadData &f){SqliteConnection db(path);test::grant(db,"demo_sysadmin","DISPATCH",1);const auto w=f.accounts.at("demo_worker01");
    for(int mode=0;mode<8;++mode){scenario("synchronized independent SQLite connections");const auto o=test::taskOrder(db,f);std::int64_t request=0;if(mode==6||mode==7)test::taskAction(db,w,o.orderId);if(mode!=0&&mode!=6)request=apply(db,w,o.orderId,mode==7?5:4).id;
        const auto version=mode>=6?5:4;std::atomic<int>attempts{0};SqliteTransaction blocker(db);
        const auto run=[&](bool second){SqliteConnection other(path);sqlite3_trace_v2(other.nativeHandle(),SQLITE_TRACE_STMT,trace,&attempts);try{
            if(mode==0){apply(other,w,o.orderId);return 201;}
            if(mode==6){event(other,w,o.orderId);return 201;}
            if(second&&mode==4){test::taskAction(other,w,o.orderId,4,"Concurrent refusal");return 200;}
            if(second&&mode==5){test::taskAction(other,w,o.orderId);return 200;}
            if(second&&mode==7){event(other,w,o.orderId,"PROGRESS_NOTE","Concurrent progress");return 201;}
            decide(other,f.accounts.at(second?"demo_sysadmin":"demo_dispatcher"),o.orderId,request,version,(mode==3||(mode==2&&second))?"REJECT":"APPROVE");return 200;
        }catch(const WorkOrderError &e){return e.status;}};
        auto one=std::async(std::launch::async,run,false),two=std::async(std::launch::async,run,true);const auto limit=std::chrono::steady_clock::now()+std::chrono::seconds(3);while(attempts<2&&std::chrono::steady_clock::now()<limit)std::this_thread::sleep_for(std::chrono::milliseconds(1));check(attempts==2,"two genuine BEGIN attempts before lock release");blocker.commit();const auto x=one.get(),y=two.get();
        if(mode==0||mode==6)check((x==201&&y==409)||(x==409&&y==201),"one created/one duplicate");else if(mode==7)check((x==200&&y==201)||(x==200&&y==403),"progress serializes before approval or loses ownership");else check((x==200&&(y==409||y==403))||(y==200&&(x==409||x==403)),"one approval/state winner only");
        const auto s=test::inspectDispatch(db,o.orderId);check(s["assignments"].size()==1&&s["dispatch_records"].size()==1,"race retains assignment and snapshot");std::cout<<"RACE mode="<<mode<<" statuses="<<x<<','<<y<<" attempts="<<attempts<<'\n';
    }test::integrity(db);
}
}
int main(int argc,char **argv){initializeCrypto();const auto root=std::filesystem::temp_directory_path()/("campusfix-workflow-"+newOpaqueId());std::filesystem::create_directories(root);int code=0;try{const auto path=root/"test.sqlite3";const auto f=test::taskFixture(path,CAMPUSFIX_MIGRATIONS_DIR,CAMPUSFIX_SEED_FILE);if(argc>1&&std::string(argv[1])=="--races")races(path,f);else services(path,f);std::cout<<"PASS workflow cases="<<cases<<" assertions="<<checks<<'\n';}catch(const std::exception &e){std::cerr<<"FAIL workflow "<<e.what()<<'\n';code=1;}std::error_code ec;std::filesystem::remove_all(root,ec);return ec?1:code;}
