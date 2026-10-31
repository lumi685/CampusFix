// Isolated executable only: fixture mutation commands and a four-thread server
// registering the production controllers. No test endpoint enters the real app.
#include "DispatchTestData.h"
#include "controllers/DispatchController.h"
#include "controllers/HealthController.h"
#include <drogon/drogon.h>
#include <fstream>
#include <iostream>
#include <mutex>
#include <thread>

using namespace campusfix;
namespace {
std::string tracePath;
std::mutex traceMutex;
int trace(unsigned,void *,void *statement,void *) {
    const auto *sql=sqlite3_sql(static_cast<sqlite3_stmt *>(statement));
    if(sql&&std::string(sql)=="BEGIN IMMEDIATE") {
        std::lock_guard<std::mutex> lock(traceMutex);
        std::ofstream file(tracePath,std::ios::app);file<<"BEGIN\n";
    }
    return 0;
}
int attach(sqlite3 *db,char **,const sqlite3_api_routines *) {
    return sqlite3_trace_v2(db,SQLITE_TRACE_STMT,trace,nullptr);
}
}
int main(int argc,char **argv) {
    try {
        initializeCrypto();
        if(argc==6&&std::string(argv[1])=="--init") {
            const auto f=test::dispatchFixture(argv[2],argv[4],argv[5]);SqliteConnection db(argv[2]);Json::Value data;
            for(const auto &a:f.accounts)data[a.first]=a.second.token;
            data["foreign"]=Json::Int64(test::dispatchOrder(db,f,13).id);
            for(const auto *mode:{"expired","revoked"}) {
                const auto token=newToken();data[mode]=token;
                AuthRepository(db).createSession(newOpaqueId(),f.accounts.at("demo_dispatcher").id,tokenDigest(token),
                    std::string(mode)=="expired"?"2020-01-01T00:00:00.000Z":"2099-01-01T00:00:00.000Z");
                if(std::string(mode)=="revoked") {auto q=db.prepare("UPDATE auth_sessions SET revoked_at='2020-01-01T00:00:00.000Z' WHERE token_hash=?1");q.bindText(1,tokenDigest(token));q.step();}
            }
            std::ofstream file(argv[3],std::ios::binary);file<<data;if(!file)throw std::runtime_error("Fixture output failed");return 0;
        }
        if(argc==4&&std::string(argv[1])=="--new") {
            SqliteConnection db(argv[2]);Json::Value credentials;std::ifstream file(argv[3]);file>>credentials;
            test::ReadData f;for(const auto *name:{"demo_reporter01","demo_dispatcher"}) {
                const auto user=AuthRepository(db).userByName(name);f.accounts[name]={user.id,credentials[name].asString()};
            }
            std::cout<<test::dispatchOrder(db,f).id;return 0;
        }
        if(argc==3&&std::string(argv[1])=="--snapshot") {SqliteConnection db(argv[2]);std::cout<<test::dispatchFingerprint(db);return 0;}
        if(argc==3&&std::string(argv[1])=="--integrity") {SqliteConnection db(argv[2]);test::integrity(db);std::cout<<"integrity_check=ok; foreign_key_check empty";return 0;}
        if(argc==4&&std::string(argv[1])=="--inspect") {SqliteConnection db(argv[2]);std::cout<<test::inspectDispatch(db,std::stoll(argv[3]));return 0;}
        if(argc==4&&std::string(argv[1])=="--mode") {
            SqliteConnection db(argv[2]);SqliteTransaction tx(db);const std::string mode=argv[3];
            if(mode=="admin-read")test::grant(db,"demo_sysadmin","BUSINESS_READ",1);
            else if(mode=="admin-area")test::grant(db,"demo_sysadmin","AREA_SCOPE",1);
            else if(mode=="admin-wrong")test::grant(db,"demo_sysadmin","DISPATCH",2);
            else if(mode=="admin-dispatch") {test::revoke(db,"demo_sysadmin","DISPATCH");test::grant(db,"demo_sysadmin","DISPATCH",3);}
            else if(mode=="admin-revoke")test::revoke(db,"demo_sysadmin","DISPATCH");
            else if(mode=="disable")db.execute("UPDATE users SET status='DISABLED' WHERE username='demo_dispatcher'");
            else if(mode=="enable")db.execute("UPDATE users SET status='ACTIVE' WHERE username='demo_dispatcher'");
            else if(mode=="none")db.execute("UPDATE workers SET availability='OFF_DUTY',active=0");
            else if(mode=="reset")db.execute("UPDATE workers SET availability='AVAILABLE',active=1,max_concurrent_tasks=3; UPDATE users SET status='ACTIVE' WHERE role='worker'; UPDATE worker_skills SET active=1,certified=1 WHERE skill_id=1; UPDATE assignments SET ended_at='2020-01-01T00:00:00.000Z',end_reason='TEST_CLEANUP' WHERE ended_at IS NULL");
            else if(mode=="capacity-one")db.execute("UPDATE workers SET max_concurrent_tasks=1 WHERE id=1");
            else if(mode=="off-duty")db.execute("UPDATE workers SET availability='OFF_DUTY' WHERE id=1");
            else if(mode=="uncertified")db.execute("UPDATE worker_skills SET certified=0 WHERE worker_id=1 AND skill_id=1");
            else if(mode=="skill-revoked")db.execute("UPDATE worker_skills SET active=0 WHERE worker_id=1 AND skill_id=1");
            else if(mode=="lower-rank")db.execute("UPDATE worker_skills SET level=1 WHERE worker_id=1 AND skill_id=1");
            else if(mode=="restore-rank")db.execute("UPDATE worker_skills SET level=5 WHERE worker_id=1 AND skill_id=1");
            else if(mode=="history-failure")db.execute("ALTER TABLE evaluations RENAME TO fixture_missing_evaluations");
            else if(mode=="history-restore")db.execute("ALTER TABLE fixture_missing_evaluations RENAME TO evaluations");
            else if(mode=="clear-failure")db.execute("DROP TRIGGER dispatch_http_failure");
            else if(mode=="big")db.execute("INSERT INTO users(id,username,password_hash,display_name,role,status) VALUES(5000000000,'fixture_big','!DISABLED!','Big worker','worker','ACTIVE'); INSERT INTO workers(id,user_id,worker_no,service_area_id,max_concurrent_tasks) VALUES(5000000000,5000000000,'FIXTURE-BIG',1,3); INSERT INTO worker_skills(worker_id,skill_id,level,certified) VALUES(5000000000,1,5,1); INSERT INTO work_orders(id,reporter_id,category_id,location_id,title,description,contact_name,contact_phone,status,approved_urgency,version) VALUES(5000000000,3,1,11,'Fictional big ID','Fictional','Fictional','00000000000','PENDING_DISPATCH','HIGH',3)");
            else if(mode=="fail-assignment"||mode=="fail-snapshot"||mode=="fail-log"||mode=="fail-update") {
                const std::string trigger=mode=="fail-update"?"BEFORE UPDATE OF status ON work_orders":
                    "BEFORE INSERT ON "+std::string(mode=="fail-assignment"?"assignments":mode=="fail-snapshot"?"dispatch_records":"status_logs");
                db.execute("CREATE TRIGGER dispatch_http_failure "+trigger+" BEGIN SELECT RAISE(ABORT,'PRIVATE_SQL_FAILURE_MARKER'); END");
            }else throw std::runtime_error("Unknown fixture mode");
            tx.commit();return 0;
        }
        if(argc==6&&std::string(argv[1])=="--hold") {
            SqliteConnection db(argv[2]);SqliteTransaction tx(db);
            {std::ofstream ready(argv[3]);ready<<"locked";}
            const auto limit=std::chrono::steady_clock::now()+std::chrono::seconds(std::stoi(argv[5]));
            while(!std::filesystem::exists(argv[4])&&std::chrono::steady_clock::now()<limit)std::this_thread::sleep_for(std::chrono::milliseconds(5));
            tx.commit();return 0;
        }
        if(argc==5&&std::string(argv[1])=="--serve") {
            tracePath=argv[4];
            if(sqlite3_auto_extension(reinterpret_cast<void(*)()>(attach))!=SQLITE_OK)throw std::runtime_error("Trace extension failed");
            registerHealthController();registerDispatchController(argv[2]);
            drogon::app().setThreadNum(4).addListener("127.0.0.1",static_cast<std::uint16_t>(std::stoi(argv[3]))).run();return 0;
        }
        return 2;
    }catch(const std::exception &){std::cerr<<"Dispatch fixture failed\n";return 1;}
}
