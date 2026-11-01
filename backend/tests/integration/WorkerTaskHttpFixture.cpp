// Isolated test executable: no test-only routes in campusfix_backend.
#include "WorkerTaskTestData.h"
#include "controllers/WorkerTaskController.h"
#include "controllers/DispatchController.h"
#include "controllers/WorkOrderQueryController.h"
#include "controllers/HealthController.h"
#include <drogon/drogon.h>
#include <fstream>
#include <iostream>
#include <mutex>
using namespace campusfix;
namespace {
std::string tracePath;
std::mutex mutex;
int trace(unsigned,void *,void *statement,void *) {
    const auto *sql=sqlite3_sql(static_cast<sqlite3_stmt *>(statement));
    if(sql&&std::string(sql)=="BEGIN IMMEDIATE"){std::lock_guard<std::mutex> lock(mutex);std::ofstream out(tracePath,std::ios::app);out<<"BEGIN\n";}
    return 0;
}
int attach(sqlite3 *db,char **,const sqlite3_api_routines *){return sqlite3_trace_v2(db,SQLITE_TRACE_STMT,trace,nullptr);}
}
int main(int argc,char **argv) {
    try {
        initializeCrypto();
        if(argc==6&&std::string(argv[1])=="--init") {
            const auto f=test::taskFixture(argv[2],argv[4],argv[5]);SqliteConnection db(argv[2]);Json::Value j;
            for(const auto &a:f.accounts)j[a.first]=a.second.token;
            for(const auto *mode:{"expired","revoked"}) {
                const auto t=newToken();j[mode]=t;AuthRepository repo(db);repo.createSession(newOpaqueId(),f.accounts.at("demo_worker01").id,tokenDigest(t),std::string(mode)=="expired"?"2020-01-01T00:00:00.000Z":"2099-01-01T00:00:00.000Z");
                if(std::string(mode)=="revoked"){auto q=db.prepare("UPDATE auth_sessions SET revoked_at='2020-01-01T00:00:00.000Z' WHERE token_hash=?1");q.bindText(1,tokenDigest(t));q.step();}
            }
            test::grant(db,"demo_sysadmin","BUSINESS_READ",1);test::grant(db,"demo_sysadmin","DISPATCH",1);
            std::ofstream out(argv[3],std::ios::binary);out<<j;if(!out)throw std::runtime_error("Fixture output failed");return 0;
        }
        if(argc==4&&std::string(argv[1])=="--new") {
            SqliteConnection db(argv[2]);Json::Value j;std::ifstream file(argv[3]);file>>j;test::ReadData f;
            for(const auto *name:{"demo_reporter01","demo_dispatcher"})f.accounts[name]={AuthRepository(db).userByName(name).id,j[name].asString()};
            std::cout<<test::dispatchOrder(db,f).id;return 0;
        }
        if(argc==3&&std::string(argv[1])=="--snapshot"){SqliteConnection db(argv[2]);std::cout<<test::dispatchFingerprint(db);return 0;}
        if(argc==3&&std::string(argv[1])=="--integrity"){SqliteConnection db(argv[2]);test::integrity(db);std::cout<<"integrity_check=ok; foreign_key_check empty";return 0;}
        if(argc==4&&std::string(argv[1])=="--inspect") {
            SqliteConnection db(argv[2]);const auto id=std::stoll(argv[3]);auto j=test::inspectDispatch(db,id);j["transfers"]=Json::arrayValue;
            auto q=db.prepare("SELECT status,reviewed_by,review_reason,resolved_at FROM transfer_requests WHERE order_id=?1");q.bindInt(1,id);
            while(q.step()){Json::Value t;t["status"]=q.columnText(0);t["reviewed_by_null"]=q.columnIsNull(1);t["review_reason"]=q.columnText(2);t["resolved_at"]=q.columnText(3);j["transfers"].append(t);}std::cout<<j;return 0;
        }
        if(argc==5&&std::string(argv[1])=="--mode") {
            SqliteConnection db(argv[2]);SqliteTransaction tx(db);const std::string mode=argv[3];const auto id=std::stoll(argv[4]);
            if(mode=="pending-transfer") {auto q=db.prepare("INSERT INTO transfer_requests(order_id,assignment_id,requested_by,reason,status,order_version) SELECT a.order_id,a.id,w.user_id,'Fictional pending request','PENDING',4 FROM assignments a JOIN workers w ON w.id=a.worker_id WHERE a.order_id=?1 AND a.ended_at IS NULL");q.bindInt(1,id);q.step();}
            else if(mode=="history") {auto q=db.prepare("INSERT INTO assignments(order_id,worker_id,assigned_by,ended_at,end_reason) SELECT ?1,2,id,'2020-01-01T00:00:00.000Z','TRANSFERRED' FROM users WHERE username='demo_dispatcher'");q.bindInt(1,id);q.step();}
            else if(mode=="disable"||mode=="enable") {auto q=db.prepare("UPDATE users SET status=?1 WHERE username='demo_worker01'");q.bindText(1,mode=="disable"?"DISABLED":"ACTIVE");q.step();}
            else if(mode=="clear-failure")db.execute("DROP TRIGGER worker_task_http_failure");
            else if(mode=="fail-assignment"||mode=="fail-order"||mode=="fail-transfer"||mode=="fail-log") {
                const auto table=mode=="fail-assignment"?"assignments":mode=="fail-order"?"work_orders":mode=="fail-transfer"?"transfer_requests":"status_logs";
                db.execute("CREATE TRIGGER worker_task_http_failure BEFORE "+std::string(mode=="fail-log"?"INSERT":"UPDATE")+" ON "+table+" BEGIN SELECT RAISE(ABORT,'PRIVATE_TASK_STORAGE_FAILURE'); END");
            }else throw std::runtime_error("Unknown fixture mode");
            tx.commit();return 0;
        }
        if(argc==5&&std::string(argv[1])=="--serve") {
            tracePath=argv[4];if(sqlite3_auto_extension(reinterpret_cast<void(*)()>(attach))!=SQLITE_OK)throw std::runtime_error("Trace setup failed");
            registerHealthController();registerDispatchController(argv[2]);registerWorkerTaskController(argv[2]);registerWorkOrderQueryController(argv[2]);
            drogon::app().setThreadNum(4).addListener("127.0.0.1",static_cast<std::uint16_t>(std::stoi(argv[3]))).run();return 0;
        }
        return 2;
    }catch(const std::exception &){std::cerr<<"Worker task fixture failed\n";return 1;}
}
