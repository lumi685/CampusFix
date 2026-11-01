// Isolated four-thread server and fixture commands. Never production test routes.
#include "WorkerTaskTestData.h"
#include "controllers/TransferController.h"
#include "controllers/WorkflowController.h"
#include "controllers/WorkerTaskController.h"
#include "controllers/DispatchController.h"
#include "controllers/WorkOrderQueryController.h"
#include "controllers/WorkOrderController.h"
#include "controllers/HealthController.h"
#include <drogon/drogon.h>
#include <fstream>
#include <iostream>
#include <mutex>
using namespace campusfix;
namespace {
std::string tracePath;std::mutex mutex;
int trace(unsigned,void *,void *s,void *){const auto *sql=sqlite3_sql(static_cast<sqlite3_stmt *>(s));if(sql&&std::string(sql)=="BEGIN IMMEDIATE"){std::lock_guard<std::mutex> lock(mutex);std::ofstream out(tracePath,std::ios::app);out<<"BEGIN\n";}return 0;}
int attach(sqlite3 *db,char **,const sqlite3_api_routines *){return sqlite3_trace_v2(db,SQLITE_TRACE_STMT,trace,nullptr);}
}
int main(int argc,char **argv){try{initializeCrypto();
    if(argc==6&&std::string(argv[1])=="--init") {
        const auto f=test::taskFixture(argv[2],argv[4],argv[5]);Json::Value data;for(const auto &a:f.accounts)data[a.first]=a.second.token;
        {SqliteConnection db(argv[2]);for(const auto *mode:{"expired","revoked"}){const auto token=newToken();data[mode]=token;AuthRepository repo(db);repo.createSession(newOpaqueId(),f.accounts.at("demo_worker01").id,tokenDigest(token),std::string(mode)=="expired"?"2020-01-01T00:00:00.000Z":"2099-01-01T00:00:00.000Z");if(std::string(mode)=="revoked"){auto q=db.prepare("UPDATE auth_sessions SET revoked_at='2020-01-01T00:00:00.000Z' WHERE token_hash=?1");q.bindText(1,tokenDigest(token));q.step();}}}
        std::ofstream out(argv[3],std::ios::binary);out<<data;if(!out)throw std::runtime_error("Fixture output");return 0;
    }
    if(argc==4&&std::string(argv[1])=="--new") {
        SqliteConnection db(argv[2]);Json::Value j;std::ifstream in(argv[3]);in>>j;test::ReadData f;
        for(const auto *name:{"demo_reporter01","demo_dispatcher"})f.accounts[name]={AuthRepository(db).userByName(name).id,j[name].asString()};
        std::cout<<test::dispatchOrder(db,f).id;return 0;
    }
    if(argc==3&&std::string(argv[1])=="--snapshot"){SqliteConnection db(argv[2]);std::cout<<test::dispatchFingerprint(db);return 0;}
    if(argc==3&&std::string(argv[1])=="--nonlog-snapshot"){
        SqliteConnection db(argv[2]);std::ostringstream out;for(const auto *table:test::dispatchTables){if(std::string(table)=="status_logs")continue;out<<table<<'|';auto columns=db.prepare(std::string("SELECT count(*) FROM pragma_table_info('")+table+"')");columns.step();const auto n=columns.columnInt(0);auto q=db.prepare(std::string("SELECT * FROM ")+table+" ORDER BY "+(std::string(table)=="worker_skills"?"1,2":"1"));while(q.step())for(int i=0;i<n;++i){if(q.columnIsNull(i))out<<"N;";else{const auto v=q.columnText(i);out<<v.size()<<':'<<v<<';';}}}std::cout<<tokenDigest(out.str());return 0;
    }
    if(argc==3&&std::string(argv[1])=="--integrity"){SqliteConnection db(argv[2]);test::integrity(db);std::cout<<"integrity_check=ok; foreign_key_check empty";return 0;}
    if(argc==4&&std::string(argv[1])=="--metrics") {SqliteConnection db(argv[2]);Json::Value j(Json::arrayValue);for(const auto &w:DispatchRepository(db).load(std::stoll(argv[3])).workers){Json::Value row;row["worker_id"]=Json::Int64(w.record.id);row["active_tasks"]=Json::Int64(w.record.activeTasks);row["samples"]=Json::arrayValue;for(const auto &s:*w.history.efficiency){Json::Value sample;sample["assignment_id"]=Json::Int64(s.assignmentId);sample["actual_minutes"]=s.actualMinutes;row["samples"].append(sample);}j.append(row);}std::cout<<j;return 0;}
    if(argc==4&&std::string(argv[1])=="--inspect"){
        SqliteConnection db(argv[2]);const auto id=std::stoll(argv[3]);auto j=test::inspectDispatch(db,id);j["transfers"]=Json::arrayValue;
        auto q=db.prepare("SELECT id,assignment_id,status,reviewed_by,review_reason,resolved_at,order_version,created_at FROM transfer_requests WHERE order_id=?1 ORDER BY id");q.bindInt(1,id);
        while(q.step()){Json::Value t;t["id"]=Json::Int64(q.columnInt(0));t["assignment_id"]=Json::Int64(q.columnInt(1));t["status"]=q.columnText(2);t["reviewed_by"]=q.columnIsNull(3)?Json::Value():Json::Value(Json::Int64(q.columnInt(3)));t["review_reason"]=q.columnIsNull(4)?Json::Value():Json::Value(q.columnText(4));t["resolved_at"]=q.columnIsNull(5)?Json::Value():Json::Value(q.columnText(5));t["order_version"]=Json::Int64(q.columnInt(6));t["created_at"]=q.columnText(7);j["transfers"].append(t);}
        auto result=db.prepare("SELECT repair_summary,rework_count,updated_at,completed_at FROM work_orders WHERE id=?1");result.bindInt(1,id);result.step();j["repair_summary"]=result.columnIsNull(0)?Json::Value():Json::Value(result.columnText(0));j["rework_count"]=Json::Int64(result.columnInt(1));j["updated_at"]=result.columnText(2);j["completed_at"]=result.columnIsNull(3)?Json::Value():Json::Value(result.columnText(3));std::cout<<j;return 0;
    }
    if(argc==5&&std::string(argv[1])=="--mode") {
        SqliteConnection db(argv[2]);SqliteTransaction tx(db);const std::string mode=argv[3];const auto id=std::stoll(argv[4]);
        if(mode=="pending") {auto q=db.prepare("INSERT INTO transfer_requests(order_id,assignment_id,requested_by,reason,order_version) SELECT a.order_id,a.id,w.user_id,'Fictional fixture application',o.version FROM assignments a JOIN workers w ON w.id=a.worker_id JOIN work_orders o ON o.id=a.order_id WHERE a.order_id=?1 AND a.ended_at IS NULL");q.bindInt(1,id);q.step();}
        else if(mode=="foreign") {auto q=db.prepare("UPDATE work_orders SET location_id=13 WHERE id=?1");q.bindInt(1,id);q.step();}
        else if(mode=="rejected") {auto q=db.prepare("UPDATE transfer_requests SET status='REJECTED',review_reason='Fictional public decision',resolved_at='2026-11-01T00:00:00.000Z' WHERE order_id=?1");q.bindInt(1,id);q.step();}
        else if(mode=="ended") {auto q=db.prepare("UPDATE assignments SET ended_at='2026-11-01T00:00:00.000Z',end_reason='TRANSFERRED' WHERE order_id=?1 AND ended_at IS NULL");q.bindInt(1,id);q.step();}
        else if(mode=="pending-verify") {auto q=db.prepare("UPDATE work_orders SET status='PENDING_VERIFY' WHERE id=?1");q.bindInt(1,id);q.step();}
        else if(mode=="completed"||mode=="closed"||mode=="cancelled") {auto q=db.prepare("UPDATE work_orders SET status=?1 WHERE id=?2");q.bindText(1,mode=="completed"?"COMPLETED":mode=="closed"?"CLOSED":"CANCELLED");q.bindInt(2,id);q.step();}
        else if(mode=="rework-fixture") {auto q=db.prepare("UPDATE work_orders SET status='IN_PROGRESS',version=version+1,rework_count=rework_count+1 WHERE id=?1 AND status='PENDING_VERIFY'");q.bindInt(1,id);q.step();}
        else if(mode=="disable-reporter"||mode=="enable-reporter") {auto q=db.prepare("UPDATE users SET status=?1 WHERE username='demo_reporter01'");q.bindText(1,mode=="disable-reporter"?"DISABLED":"ACTIVE");q.step();}
        else if(mode=="version-limit"||mode=="rework-limit"||mode=="assignment-limit") {auto q=db.prepare(mode=="version-limit"?"UPDATE work_orders SET version=9223372036854775807 WHERE id=?1":mode=="rework-limit"?"UPDATE work_orders SET rework_count=9223372036854775807 WHERE id=?1":"UPDATE assignments SET version=9223372036854775807 WHERE order_id=?1");q.bindInt(1,id);q.step();}
        else if(mode=="old-assignment") {
            auto q=db.prepare("UPDATE assignments SET ended_at='2026-11-01T00:00:00.000Z',end_reason='TRANSFERRED' WHERE order_id=?1 AND ended_at IS NULL");q.bindInt(1,id);q.step();
            auto insert=db.prepare("INSERT INTO assignments(order_id,worker_id,assigned_by) SELECT ?1,2,id FROM users WHERE username='demo_dispatcher'");insert.bindInt(1,id);insert.step();
        }
        else if(mode=="clear-failure")db.execute("DROP TRIGGER workflow_http_failure");
        else if(mode=="fail-insert-transfer"||mode=="fail-update-transfer"||mode=="fail-assignment"||mode=="fail-order"||mode=="fail-log"){
            const auto table=mode=="fail-assignment"?"assignments":mode=="fail-order"?"work_orders":mode=="fail-log"?"status_logs":"transfer_requests";const auto action=mode=="fail-log"||mode=="fail-insert-transfer"?"INSERT":"UPDATE";
            db.execute("CREATE TRIGGER workflow_http_failure BEFORE "+std::string(action)+" ON "+table+" BEGIN SELECT RAISE(ABORT,'PRIVATE_WORKFLOW_FAILURE'); END");
        }
        else if(mode=="admin-dispatch")test::grant(db,"demo_sysadmin","DISPATCH",1);
        else if(mode=="admin-read")test::grant(db,"demo_sysadmin","BUSINESS_READ",1);
        else if(mode=="remove-admin-read")test::revoke(db,"demo_sysadmin","BUSINESS_READ");
        else if(mode=="overlap")test::grant(db,"demo_sysadmin","DISPATCH",3);
        else if(mode=="revoke")test::revoke(db,"demo_sysadmin","DISPATCH");
        else if(mode=="disable"||mode=="enable") {auto q=db.prepare("UPDATE users SET status=?1 WHERE username='demo_worker01'");q.bindText(1,mode=="disable"?"DISABLED":"ACTIVE");q.step();}
        else throw std::runtime_error("Unknown fixture mode");tx.commit();return 0;
    }
    if(argc==5&&std::string(argv[1])=="--serve") {
        tracePath=argv[4];if(sqlite3_auto_extension(reinterpret_cast<void(*)()>(attach))!=SQLITE_OK)throw std::runtime_error("Trace extension");
        registerHealthController();registerTransferController(argv[2]);registerWorkflowController(argv[2]);registerDispatchController(argv[2]);registerWorkerTaskController(argv[2]);registerWorkOrderController(argv[2]);
        drogon::app().setThreadNum(4).addListener("127.0.0.1",static_cast<std::uint16_t>(std::stoi(argv[3]))).run();return 0;
    }
    return 2;
}catch(const std::exception &e){std::cerr<<"Workflow fixture failed: "<<e.what()<<'\n';return 1;}}
