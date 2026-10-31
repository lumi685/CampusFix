#include "DispatchTestData.h"
#include "repositories/AssignmentRepository.h"
#include "dispatch/WeightedScoreStrategy.h"
#include <atomic>
#include <chrono>
#include <future>
#include <iostream>
#include <limits>
#include <thread>

using namespace campusfix;
namespace {
int assertions=0,cases=0;
void check(bool ok,const std::string &what) {if(!ok)throw std::runtime_error(what);++assertions;}
void scenario(const std::string &name) {++cases;std::cout<<"CASE "<<name<<'\n';}
AssignmentInput input(std::int64_t worker=1,std::int64_t version=3,std::optional<std::string> reason={}) {return {worker,version,std::move(reason)};}
Json::Value preview(SqliteConnection &db,const test::ReadAccount &a,std::int64_t id) {return DispatchService(db).recommend(a.id,a.token,id);}
AssignmentResult assign(SqliteConnection &db,const test::ReadAccount &a,std::int64_t id,const AssignmentInput &i) {return AssignmentService(db).assign(a.id,a.token,id,i,"dispatch-service-request");}
void rejected(SqliteConnection &db,const test::ReadAccount &a,std::int64_t id,const AssignmentInput &i,int status,const std::string &code={}) {
    const auto before=test::dispatchFingerprint(db);bool refused=false;
    try {assign(db,a,id,i);}catch(const WorkOrderError &e){refused=e.status==status&&(code.empty()||e.code==code);}
    check(refused,"expected assignment rejection "+std::to_string(status)+" "+code);check(before==test::dispatchFingerprint(db),"rejection changes no business column");
}
void previewDenied(SqliteConnection &db,const test::ReadAccount &a,std::int64_t id,int status) {
    const auto before=test::dispatchFingerprint(db);bool refused=false;try {preview(db,a,id);}catch(const WorkOrderError &e){refused=e.status==status;}
    check(refused,"expected preview rejection "+std::to_string(status));check(before==test::dispatchFingerprint(db),"preview rejection remains strictly read-only");
}
int traceBegin(unsigned type,void *context,void *statement,void *) {
    if(type==SQLITE_TRACE_STMT) {const auto *sql=sqlite3_sql(static_cast<sqlite3_stmt *>(statement));if(sql&&std::string(sql)=="BEGIN IMMEDIATE")++*static_cast<std::atomic<int> *>(context);}return 0;
}
void awaitAttempts(std::atomic<int> &attempts,int expected) {
    const auto limit=std::chrono::steady_clock::now()+std::chrono::seconds(3);
    while(attempts.load()!=expected&&std::chrono::steady_clock::now()<limit)std::this_thread::sleep_for(std::chrono::milliseconds(1));
    check(attempts.load()==expected,"independent connections actually entered BEGIN IMMEDIATE while another connection held the SQLite write lock");
}
void runRaces(const std::filesystem::path &path,const test::ReadData &f) {
    SqliteConnection db(path);const auto a=f.accounts.at("demo_dispatcher");
    for(const auto mode:{0,1,2}) {
        scenario(mode==0?"same order different worker":mode==1?"same order same worker":"two orders last worker capacity");
        const auto first=test::dispatchOrder(db,f),second=mode==2?test::dispatchOrder(db,f):first;
        db.execute("UPDATE assignments SET ended_at='2020-01-01T00:00:00.000Z',end_reason='TEST_CLEANUP' WHERE ended_at IS NULL");
        db.execute(mode==2?"UPDATE workers SET max_concurrent_tasks=1":"UPDATE workers SET max_concurrent_tasks=3");
        const auto before=db.scalarInt("SELECT count(*) FROM assignments"),records=db.scalarInt("SELECT count(*) FROM dispatch_records"),logs=db.scalarInt("SELECT count(*) FROM status_logs");
        std::atomic<int> attempts{0};SqliteTransaction blocker(db);
        const auto run=[&](bool other) {
            SqliteConnection connection(path);sqlite3_trace_v2(connection.nativeHandle(),SQLITE_TRACE_STMT,traceBegin,&attempts);
            try {assign(connection,a,other?second.id:first.id,input(other&&mode==0?2:1));return 200;}
            catch(const WorkOrderError &e){return e.status;}
        };
        auto one=std::async(std::launch::async,run,false),two=std::async(std::launch::async,run,true);
        awaitAttempts(attempts,2);blocker.commit();const int x=one.get(),y=two.get();
        const int wins=(x==200)+(y==200);check(wins==1,"exactly one winner");check((x==200?(y==409||y==422):(x==409||x==422)),"loser is a defined business conflict, not 500");
        check(db.scalarInt("SELECT count(*) FROM assignments")==before+1&&db.scalarInt("SELECT count(*) FROM dispatch_records")==records+1&&db.scalarInt("SELECT count(*) FROM status_logs")==logs+1,"one atomic assignment/snapshot/log and no loser side effects");
        check(db.scalarInt("SELECT count(*) FROM assignments WHERE ended_at IS NULL")==1,"one effective assignment survives");
        if(mode==2) {auto q=db.prepare("SELECT sum(status='PENDING_ACCEPT'),sum(status='PENDING_DISPATCH'),sum(version=4),sum(version=3) FROM work_orders WHERE id IN (?1,?2)");q.bindInt(1,first.id);q.bindInt(2,second.id);q.step();check(q.columnInt(0)==1&&q.columnInt(1)==1&&q.columnInt(2)==1&&q.columnInt(3)==1,"losing order retains original status and version");}
        std::cout<<"RACE mode="<<mode<<" sqlite_begin_attempts="<<attempts<<" winners="<<wins<<" statuses="<<x<<','<<y<<" fingerprint="<<test::dispatchFingerprint(db)<<'\n';
    }
    for(const bool revokeToken:{false,true}) {
        scenario(revokeToken?"session revoked before waiting writer obtains lock":"grant revoked before waiting writer obtains lock");
        const auto order=test::dispatchOrder(db,f);std::atomic<int> attempts{0};SqliteTransaction blocker(db);
        auto request=std::async(std::launch::async,[&]{SqliteConnection other(path);sqlite3_trace_v2(other.nativeHandle(),SQLITE_TRACE_STMT,traceBegin,&attempts);try{assign(other,a,order.id,input());return 200;}catch(const WorkOrderError &e){return e.status;}});
        awaitAttempts(attempts,1);
        if(revokeToken) {auto q=db.prepare("UPDATE auth_sessions SET revoked_at='2020-01-01T00:00:00.000Z' WHERE token_hash=?1");q.bindText(1,tokenDigest(a.token));q.step();}
        else test::revoke(db,"demo_dispatcher","AREA_SCOPE");
        blocker.commit();const auto before=test::dispatchFingerprint(db);check(request.get()==(revokeToken?401:404),"transaction rechecks latest identity/grant after waiting");check(test::dispatchFingerprint(db)==before,"waiting unauthorized request creates no partial rows");
        if(!revokeToken)test::grant(db,"demo_dispatcher","AREA_SCOPE",1);
    }
    test::integrity(db);
}
void services(const std::filesystem::path &path,const test::ReadData &f) {
    SqliteConnection db(path);const auto a=f.accounts.at("demo_dispatcher"),admin=f.accounts.at("demo_sysadmin");const auto order=test::dispatchOrder(db,f);
    scenario("real six candidates, independent score recomputation and full-table read-only preview");
    const auto before=test::dispatchFingerprint(db);
    const auto dto=preview(db,a,order.id);
    check(before==test::dispatchFingerprint(db),"all 16 business tables unchanged after successful GET composition");
    check(dto["order_id"].asInt64()==order.id&&dto["order_version"].asInt64()==3,"true order and version");
    check(dto["top_candidates"].size()==3&&dto["other_eligible_candidates"].size()==3,"top three and every remaining eligible worker returned");
    check(dto["top_candidates"][0]["worker_id"].asInt64()==1,"actual strongest certified skill ranks first");
    for(const auto &group:{"top_candidates","other_eligible_candidates"})for(const auto &c:dto[group]) {
        const auto &d=c["score_detail"];const double sum=d["skill"].asDouble()*.35+d["area"].asDouble()*.20+d["load"].asDouble()*.20+d["efficiency"].asDouble()*.10+d["rating"].asDouble()*.10+d["duty"].asDouble()*.05;
        check(std::abs(std::round(sum*10)/10-c["total_score"].asDouble())<.001,"returned components independently reproduce rounded total");
        check(c["sample_counts"]["efficiency"].asInt()==0&&c["sample_counts"]["rating"].asInt()==0,"real loaded zero sample counts");
    }
    scenario("identity, resource visibility and explicit capability scopes");
    previewDenied(db,f.accounts.at("demo_reporter01"),order.id,403);previewDenied(db,f.accounts.at("demo_worker01"),order.id,404);
    previewDenied(db,admin,order.id,404);const auto foreign=test::dispatchOrder(db,f,13);previewDenied(db,a,foreign.id,404);
    test::grant(db,"demo_sysadmin","BUSINESS_READ",1);previewDenied(db,admin,order.id,403);
    test::grant(db,"demo_sysadmin","AREA_SCOPE",1);previewDenied(db,admin,order.id,403);
    test::grant(db,"demo_sysadmin","DISPATCH",2);previewDenied(db,admin,order.id,403);test::revoke(db,"demo_sysadmin","DISPATCH");
    test::grant(db,"demo_sysadmin","DISPATCH",3);check(preview(db,admin,order.id)["top_candidates"].size()==3,"own capability building scope works");test::revoke(db,"demo_sysadmin","DISPATCH");previewDenied(db,admin,order.id,403);
    scenario("invalid or disabled sessions cannot reach domain data");
    auto invalid=a;invalid.token=newToken();previewDenied(db,invalid,order.id,401);
    db.execute("UPDATE users SET status='DISABLED' WHERE username='demo_dispatcher'");previewDenied(db,a,order.id,401);db.execute("UPDATE users SET status='ACTIVE' WHERE username='demo_dispatcher'");
    scenario("input validation, version, status, and public error codes");
    rejected(db,a,order.id,input(1,2),409,"ORDER_VERSION_CONFLICT");rejected(db,a,order.id,input(0),422);rejected(db,a,order.id,input(1,0),422);
    rejected(db,a,order.id,input(1,3,std::string(501,'x')),422);rejected(db,a,order.id,input(1,3,std::string("\xC0\x80")),422);
    for(const auto *state:{"DRAFT","PENDING_REVIEW","PENDING_SUPPLEMENT","IN_PROGRESS","PENDING_VERIFY","COMPLETED","CLOSED","CANCELLED"}) {
        auto q=db.prepare("UPDATE work_orders SET status=?1 WHERE id=?2");q.bindText(1,state);q.bindInt(2,order.id);q.step();previewDenied(db,a,order.id,409);rejected(db,a,order.id,input(),409,"ORDER_STATUS_CONFLICT");
    }
    {auto q=db.prepare("UPDATE work_orders SET status='PENDING_DISPATCH' WHERE id=?1");q.bindInt(1,order.id);q.step();}
    scenario("selected current fourth candidate requires a nonblank reason");
    const auto fourth=preview(db,a,order.id)["other_eligible_candidates"][0]["worker_id"].asInt64();rejected(db,a,order.id,input(fourth),422);rejected(db,a,order.id,input(fourth,3,std::string("　 \t")),422);
    auto fourthOrder=test::dispatchOrder(db,f);const auto fourthResult=assign(db,a,fourthOrder.id,input(fourth,3,"Fictional manual selection reason"));check(fourthResult.status=="PENDING_ACCEPT"&&fourthResult.version==4,"rank four with explicit reason succeeds");
    scenario("top-three preview is not authority: changed skills force final rank and reason recalculation");
    auto moved=test::dispatchOrder(db,f);check(preview(db,a,moved.id)["top_candidates"][0]["worker_id"].asInt64()==1,"initial preview top one");
    db.execute("UPDATE worker_skills SET level=1 WHERE worker_id=1 AND skill_id=1");rejected(db,a,moved.id,input(),422,"VALIDATION_ERROR");
    const auto movedResult=assign(db,a,moved.id,input(1,3,"Reason for current lower rank"));
    {auto q=db.prepare("SELECT candidates_json FROM dispatch_records WHERE assignment_id=?1");q.bindInt(1,movedResult.assignmentId);q.step();Json::Value rows;Json::CharReaderBuilder b;std::istringstream stream(q.columnText(0));std::string errors;check(Json::parseFromStream(b,stream,&rows,&errors),"final candidate snapshot parses");bool found=false;for(const auto &row:rows)if(row["worker_id"].asInt64()==1)found=row["rank"].asInt()>3&&row["score_detail"]["skill"].asDouble()==20;check(found,"persisted snapshot reflects transaction-time skill and current non-Top3 rank");check(rows.size()==6,"snapshot preserves other eligible candidates");}
    db.execute("UPDATE worker_skills SET level=5 WHERE worker_id=1 AND skill_id=1");
    scenario("latest OFF_DUTY, certificate, skill and capacity facts cannot be bypassed");
    for(const auto &pair:std::vector<std::pair<std::string,std::string>>{{"UPDATE workers SET availability='OFF_DUTY' WHERE id=1","UPDATE workers SET availability='AVAILABLE' WHERE id=1"},{"UPDATE worker_skills SET certified=0 WHERE worker_id=1 AND skill_id=1","UPDATE worker_skills SET certified=1 WHERE worker_id=1 AND skill_id=1"},{"UPDATE worker_skills SET active=0 WHERE worker_id=1 AND skill_id=1","UPDATE worker_skills SET active=1 WHERE worker_id=1 AND skill_id=1"},{"UPDATE workers SET max_concurrent_tasks=1 WHERE id=1","UPDATE workers SET max_concurrent_tasks=3 WHERE id=1"}}) {
        auto target=test::dispatchOrder(db,f);db.execute(pair.first);rejected(db,a,target.id,input(),422,"WORKER_INELIGIBLE");db.execute(pair.second);
    }
    scenario("successful assignment links assignment, final score snapshot and state audit atomically");
    const auto target=test::dispatchOrder(db,f);const auto counts=db.scalarInt("SELECT count(*) FROM status_logs");const auto assigned=assign(db,a,target.id,input(2));const auto inspected=test::inspectDispatch(db,target.id);
    check(assigned.status=="PENDING_ACCEPT"&&assigned.version==4&&assigned.workerId==2&&assigned.orderId==target.id,"state/version/worker response");
    check(inspected["assignments"].size()==1&&inspected["dispatch_records"].size()==1&&db.scalarInt("SELECT count(*) FROM status_logs")==counts+1,"exactly one assignment/snapshot/audit");
    check(inspected["dispatch_records"][0]["assignment_id"].asString()==std::to_string(assigned.assignmentId),"snapshot foreign key binds actual assignment");
    const auto &last=inspected["status_logs"][inspected["status_logs"].size()-1];check(last["assignment_id"].asString()==std::to_string(assigned.assignmentId)&&last["action"]=="ORDER_ASSIGNED"&&last["actor_id"].asString()==std::to_string(a.id),"audit assignment and authenticated actor");
    rejected(db,a,target.id,input(2),409,"ACTIVE_ASSIGNMENT_CONFLICT");previewDenied(db,a,target.id,409);
    scenario("database partial unique index independently rejects another effective assignment");
    {const auto beforeIndex=test::dispatchFingerprint(db);bool refused=false;
        try{AssignmentRepository(db).insert(target.id,1,a.id,{},"2020-01-01T00:00:00.000Z");}
        catch(const SqliteError &e){refused=e.primaryCode()==SQLITE_CONSTRAINT;}
        check(refused,"real index rejects duplicate even without the service precheck");
        check(beforeIndex==test::dispatchFingerprint(db),"constraint failure has no data side effects");}
    scenario("int64 version overflow rejected without writes");
    auto overflow=test::dispatchOrder(db,f);{auto q=db.prepare("UPDATE work_orders SET version=?1 WHERE id=?2");q.bindInt(1,std::numeric_limits<std::int64_t>::max());q.bindInt(2,overflow.id);q.step();}rejected(db,a,overflow.id,input(2,std::numeric_limits<std::int64_t>::max()),409,"ORDER_VERSION_CONFLICT");
    scenario("three storage failures roll back every business column");
    for(const auto *table:{"assignments","dispatch_records","status_logs"}) {
        const auto o=test::dispatchOrder(db,f);const auto fingerprint=test::dispatchFingerprint(db);db.execute(std::string("CREATE TRIGGER dispatch_failure BEFORE INSERT ON ")+table+" BEGIN SELECT RAISE(ABORT,'PRIVATE_SQL_FAILURE_MARKER'); END");bool failed=false;
        try {assign(db,a,o.id,input(2));}catch(const SqliteError &){failed=true;}check(failed,"injected failure reaches real SQL at "+std::string(table));check(fingerprint==test::dispatchFingerprint(db),"all data rolled back at "+std::string(table));db.execute("DROP TRIGGER dispatch_failure");
    }
    scenario("history read failure cannot become cold start or leave partial writes");
    {auto o=test::dispatchOrder(db,f);db.execute("ALTER TABLE evaluations RENAME TO fixture_unavailable_evaluations");bool failed=false;try{assign(db,a,o.id,input(2));}catch(const std::exception &){failed=true;}check(failed,"unavailable historical table blocks assignment");db.execute("ALTER TABLE fixture_unavailable_evaluations RENAME TO evaluations");rejected(db,a,o.id,input(2,2),409);check(test::inspectDispatch(db,o.id)["assignments"].empty(),"no assignment created by failed history read");}
    scenario("in-transaction grant/session removal rolls entire assignment back");
    for(const bool token:{false,true}) {auto o=test::dispatchOrder(db,f);const auto fingerprint=test::dispatchFingerprint(db);db.execute(token?"CREATE TRIGGER dispatch_revoke AFTER INSERT ON assignments BEGIN UPDATE auth_sessions SET revoked_at='2020-01-01T00:00:00.000Z' WHERE user_id=NEW.assigned_by; END":"CREATE TRIGGER dispatch_revoke AFTER INSERT ON assignments BEGIN UPDATE management_grants SET revoked_at='2020-01-01T00:00:00.000Z' WHERE user_id=NEW.assigned_by; END");bool failed=false;try{assign(db,a,o.id,input(2));}catch(const WorkOrderError &e){failed=e.status==(token?401:404);}check(failed,"identity and grant checked before commit");check(fingerprint==test::dispatchFingerprint(db),"revocation trigger and all business effects roll back together");db.execute("DROP TRIGGER dispatch_revoke");}
    test::integrity(db);check(true,"integrity_check=ok foreign_key_check empty");
}
}
int main(int argc,char **argv) {
    const auto root=std::filesystem::temp_directory_path()/("campusfix-dispatch-service-"+newOpaqueId());std::filesystem::create_directories(root);int result=0;
    try {const auto path=root/"test.sqlite3";const auto f=test::dispatchFixture(path,CAMPUSFIX_MIGRATIONS_DIR,CAMPUSFIX_SEED_FILE);if(argc>1&&std::string(argv[1])=="--concurrency")runRaces(path,f);else services(path,f);std::cout<<"PASS dispatch integration cases="<<cases<<" assertions="<<assertions<<'\n';}
    catch(const std::exception &e){std::cerr<<"FAIL dispatch integration: "<<e.what()<<'\n';result=1;}
    std::error_code error;std::filesystem::remove_all(root,error);if(error){std::cerr<<"Temporary fixture cleanup failed\n";result=1;}return result;
}
