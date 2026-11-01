#include "WorkerTaskTestData.h"
#include "services/WorkOrderService.h"
#include "domain/WorkOrderValidation.h"
#include <atomic>
#include <future>
#include <iostream>
#include <limits>
#include <thread>
using namespace campusfix;
namespace {
int cases=0,assertions=0;
void check(bool value,const std::string &message){if(!value)throw std::runtime_error(message);++assertions;}
void scenario(const char *name){++cases;std::cout<<"CASE "<<name<<'\n';}
void deny(SqliteConnection &db,const test::ReadAccount &a,std::int64_t id,std::int64_t version,std::optional<std::string> reason,int status){const auto before=test::dispatchFingerprint(db);bool rejected=false;try{test::taskAction(db,a,id,version,reason);}catch(const WorkOrderError &e){rejected=e.status==status;}check(rejected,"expected safe rejection "+std::to_string(status));check(before==test::dispatchFingerprint(db),"rejected request leaves all16 tables unchanged");}
void services(const std::filesystem::path &path,const test::ReadData &f){
    SqliteConnection db(path);const auto worker=f.accounts.at("demo_worker01");
    scenario("refusal, pending transfer superseded, capacity 1 to 0 and full history retained");
    const auto refused=test::taskOrder(db,f);check(test::activeTaskCount(db)==1,"actual initial load1");test::pendingTransfer(db,refused,worker.id);
    const auto result=test::taskAction(db,worker,refused.orderId,4,"Fictional refusal reason");check(result.status=="PENDING_DISPATCH"&&result.version==5&&result.assignmentEnded,"refusal response");check(test::activeTaskCount(db)==0,"actual load0, no counter field");
    const auto state=test::inspectDispatch(db,refused.orderId);const auto &a=state["assignments"][0],&log=state["status_logs"][state["status_logs"].size()-1];
    check(state["assignments"].size()==1&&state["dispatch_records"].size()==1,"original assignment/snapshot retained");check(a["end_reason"]=="REFUSED"&&!a["ended_at"].isNull()&&a["accepted_at"].isNull()&&a["version"]=="2","refused assignment updated not replaced");check(a["assign_reason"]=="Fictional test assignment","original manager reason preserved");check(log["public_note"]=="Fictional refusal reason"&&log["action"]=="ORDER_REFUSED"&&log["assignment_id"]==std::to_string(refused.assignmentId),"refusal reason and real assignment audit");
    {auto q=db.prepare("SELECT status,reviewed_by,review_reason,resolved_at FROM transfer_requests WHERE assignment_id=?1");q.bindInt(1,refused.assignmentId);q.step();check(q.columnText(0)=="SUPERSEDED"&&q.columnIsNull(1)&&!q.columnText(2).empty()&&!q.columnIsNull(3),"pending request resolved by system in same transaction");}
    deny(db,worker,refused.orderId,4,"Again",409);
    const auto preview=DispatchService(db).recommend(f.accounts.at("demo_dispatcher").id,f.accounts.at("demo_dispatcher").token,refused.orderId);check(preview["top_candidates"][0]["worker_id"].asInt64()==1&&preview["top_candidates"][0]["active_tasks"].asInt64()==0,"refused worker can legally be recommended again");
    scenario("accept updates timestamps and both versions with no new assignment or extra load");
    const auto accepted=test::taskOrder(db,f);const auto load=test::activeTaskCount(db);const auto counts=db.scalarInt("SELECT count(*) FROM assignments");const auto acceptedResult=test::taskAction(db,worker,accepted.orderId);
    check(acceptedResult.status=="IN_PROGRESS"&&acceptedResult.version==5&&validUtcDateTime(acceptedResult.acceptedAt),"real accept response/time");const auto acceptedState=test::inspectDispatch(db,accepted.orderId);
    check(acceptedState["assignments"][0]["accepted_at"]==acceptedResult.acceptedAt&&acceptedState["assignments"][0]["version"]=="2"&&acceptedState["assignments"][0]["ended_at"].isNull(),"same effective assignment accepted");check(test::activeTaskCount(db)==load&&db.scalarInt("SELECT count(*) FROM assignments")==counts,"accept creates no new task/load");check(acceptedState["status_logs"][acceptedState["status_logs"].size()-1]["action"]=="ORDER_ACCEPTED","stable action code");deny(db,worker,accepted.orderId,4,{},409);deny(db,worker,accepted.orderId,5,"Too late",409);
    scenario("current owner not reporter or manager, with historical visibility separated");
    const auto o=test::taskOrder(db,f);deny(db,f.accounts.at("demo_worker02"),o.orderId,4,{},404);deny(db,f.accounts.at("demo_reporter01"),o.orderId,4,{},403);deny(db,f.accounts.at("demo_dispatcher"),o.orderId,4,{},403);test::grant(db,"demo_sysadmin","BUSINESS_READ",1);test::grant(db,"demo_sysadmin","DISPATCH",1);deny(db,f.accounts.at("demo_sysadmin"),o.orderId,4,"No impersonation",403);
    {auto q=db.prepare("INSERT INTO assignments(order_id,worker_id,assigned_by,ended_at,end_reason) VALUES(?1,2,?2,'2020-01-01T00:00:00.000Z','TRANSFERRED')");q.bindInt(1,o.orderId);q.bindInt(2,f.accounts.at("demo_dispatcher").id);q.step();}
    deny(db,f.accounts.at("demo_worker02"),o.orderId,4,{},403);deny(db,f.accounts.at("demo_worker02"),o.orderId,4,"History is not ownership",403);
    scenario("current-only list excludes historical owners while default assigned stays historical");
    OrderQuery query;query.view=OrderView::Assigned;query.status="PENDING_ACCEPT";query.pageSize=100;const auto &other=f.accounts.at("demo_worker02");
    const auto historical=WorkOrderService(db).list(other.id,other.token,query);check(historical.total==1&&historical.items[0].id==o.orderId,"default history preserved");query.currentOnly=true;check(WorkOrderService(db).list(other.id,other.token,query).total==0,"server filters current ownership, before pagination");check(WorkOrderService(db).list(worker.id,worker.token,query).total>=1,"current owner retains task");
    scenario("reason length, Unicode whitespace, UTF8 and state/version invalid inputs");
    for(const auto &reason:std::vector<std::string>{""," \t\n",u8"\u3000\u00a0\u2009",std::string(501,'x'),std::string("\xC0\x80"),std::string(1,'\0')})deny(db,worker,o.orderId,4,reason,422);
    deny(db,worker,o.orderId,3,{},409);deny(db,worker,o.orderId,0,{},422);
    scenario("disabled, invalid, expired or revoked sessions cannot mutate");
    auto invalid=worker;invalid.token=newToken();deny(db,invalid,o.orderId,4,{},401);
    db.execute("UPDATE users SET status='DISABLED' WHERE username='demo_worker01'");deny(db,worker,o.orderId,4,{},401);db.execute("UPDATE users SET status='ACTIVE' WHERE username='demo_worker01'");
    for(const bool revoked:{false,true}){auto account=worker;account.token=newToken();AuthRepository(db).createSession(newOpaqueId(),account.id,tokenDigest(account.token),revoked?"2099-01-01T00:00:00.000Z":"2020-01-01T00:00:00.000Z");if(revoked){auto q=db.prepare("UPDATE auth_sessions SET revoked_at='2020-01-01T00:00:00.000Z' WHERE token_hash=?1");q.bindText(1,tokenDigest(account.token));q.step();}deny(db,account,o.orderId,4,{},401);}
    scenario("int64 order and assignment version overflow guards");
    for(const bool assignment:{false,true}){const auto target=test::taskOrder(db,f);auto q=db.prepare(assignment?"UPDATE assignments SET version=?1 WHERE id=?2":"UPDATE work_orders SET version=?1 WHERE id=?2");q.bindInt(1,std::numeric_limits<std::int64_t>::max());q.bindInt(2,assignment?target.assignmentId:target.orderId);q.step();deny(db,worker,target.orderId,assignment?4:std::numeric_limits<std::int64_t>::max(),{},409);}
    scenario("accept/refuse storage failures roll back orders, assignments, transfers and logs");
    for(const bool refuse:{false,true})for(const auto *table:{"assignments","work_orders","status_logs","transfer_requests"}){
        if(!refuse&&std::string(table)=="transfer_requests")continue;const auto target=test::taskOrder(db,f);test::pendingTransfer(db,target,worker.id);const auto before=test::dispatchFingerprint(db);
        const std::string event=std::string(table)=="status_logs"?"INSERT":"UPDATE";db.execute("CREATE TRIGGER task_failure BEFORE "+event+" ON "+table+" BEGIN SELECT RAISE(ABORT,'PRIVATE_TASK_FAILURE'); END");bool failed=false;try{test::taskAction(db,worker,target.orderId,4,refuse?std::optional<std::string>{"Fixture refuse"}:std::nullopt);}catch(const SqliteError &){failed=true;}
        check(failed,"injected SQL failure reached");check(test::dispatchFingerprint(db)==before,"every table restored after failure");db.execute("DROP TRIGGER task_failure");
    }
    test::integrity(db);check(true,"integrity ok, FK check empty");
}
int trace(unsigned,void *context,void *statement,void *){const auto *sql=sqlite3_sql(static_cast<sqlite3_stmt *>(statement));if(sql&&std::string(sql)=="BEGIN IMMEDIATE")++*static_cast<std::atomic<int> *>(context);return 0;}
void races(const std::filesystem::path &path,const test::ReadData &f){SqliteConnection db(path);for(int mode=0;mode<3;++mode){scenario("synchronized accept/refuse independent SQLite connections");const auto o=test::taskOrder(db,f);const auto before=db.scalarInt("SELECT count(*) FROM status_logs");std::atomic<int>attempts{0};SqliteTransaction blocker(db);
    const auto run=[&](bool second){SqliteConnection other(path);sqlite3_trace_v2(other.nativeHandle(),SQLITE_TRACE_STMT,trace,&attempts);try{test::taskAction(other,f.accounts.at("demo_worker01"),o.orderId,4,(mode==1||(mode==2&&second))?std::optional<std::string>{"Concurrent refusal"}:std::nullopt);return 200;}catch(const WorkOrderError &e){return e.status;}};
    auto one=std::async(std::launch::async,run,false),two=std::async(std::launch::async,run,true);const auto until=std::chrono::steady_clock::now()+std::chrono::seconds(3);while(attempts.load()!=2&&std::chrono::steady_clock::now()<until)std::this_thread::sleep_for(std::chrono::milliseconds(1));check(attempts==2,"two real BEGIN attempts before release");blocker.commit();const int x=one.get(),y=two.get();check((x==200)+(y==200)==1&&((x==200&&y==409)||(y==200&&x==409)),"one winner, one defined conflict");check(db.scalarInt("SELECT count(*) FROM status_logs")==before+1,"no loser log");const auto state=test::inspectDispatch(db,o.orderId);check(state["version"]==5&&state["assignments"].size()==1&&state["assignments"][0]["version"]=="2","one atomic version increment");std::cout<<"RACE mode="<<mode<<" attempts="<<attempts<<" statuses="<<x<<','<<y<<" fingerprint="<<test::dispatchFingerprint(db)<<'\n';}test::integrity(db);}
}
int main(int argc,char **argv){const auto root=std::filesystem::temp_directory_path()/("campusfix-worker-task-"+newOpaqueId());std::filesystem::create_directories(root);int result=0;try{const auto path=root/"test.sqlite3";const auto f=test::taskFixture(path,CAMPUSFIX_MIGRATIONS_DIR,CAMPUSFIX_SEED_FILE);if(argc>1&&std::string(argv[1])=="--races")races(path,f);else services(path,f);std::cout<<"PASS worker task cases="<<cases<<" assertions="<<assertions<<'\n';}catch(const std::exception &e){std::cerr<<"FAIL worker task "<<e.what()<<'\n';result=1;}std::error_code ec;std::filesystem::remove_all(root,ec);return ec?1:result;}
