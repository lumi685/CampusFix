// Local test fixture utility, not linked into the production server.
#include "infrastructure/DatabaseMigration.h"
#include "infrastructure/DemoSeed.h"
#include "infrastructure/AuthCrypto.h"
#include "services/DemoAccountActivation.h"
#include "repositories/AuthRepository.h"
#include <json/json.h>
#include <fstream>
#include <iostream>
#include <thread>
#include <chrono>

using namespace campusfix;
int main(int argc,char **argv) {
    try {
        initializeCrypto();
        if(argc==5&&std::string(argv[1])=="--hold-write") {
            SqliteConnection db(argv[2]);SqliteTransaction tx(db);
            {std::ofstream ready(argv[3]);ready<<"ready";}
            for(int i=0;i<300&&!std::filesystem::exists(argv[4]);++i)
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
            return 0; // Roll back the test-only lock transaction.
        }
        if(argc==5) {
            applyMigrations(argv[1],argv[3]);applyDemoSeed(argv[1],argv[4]);
            Json::Value data(Json::objectValue);const auto password=newToken();
            setDemoPassword(argv[1],"demo_reporter01",password);enableDemoAccount(argv[1],"demo_reporter01");
            data["password"]=password;
            SqliteConnection db(argv[1]);AuthRepository repo(db);
            db.execute("UPDATE users SET status='ACTIVE' WHERE username IN ('demo_worker01','demo_reporter02','demo_dispatcher','demo_sysadmin')");
            for(const auto &name:{"demo_reporter01","demo_reporter02","demo_worker01","demo_dispatcher","demo_sysadmin"}) {
                const auto user=repo.userByName(name);const auto token=newToken();
                repo.createSession(newOpaqueId(),user.id,tokenDigest(token),"2099-01-01T00:00:00.000Z");
                data[name]=token;
            }
            data["category_id"]=Json::Int64(db.scalarInt("SELECT id FROM fault_categories WHERE code='DEMO-SOCKET'"));
            data["location_id"]=Json::Int64(db.scalarInt("SELECT id FROM locations WHERE code='DEMO-A1-101'"));
            std::ofstream f(argv[2],std::ios::binary);f<<data;
            if(!f) throw std::runtime_error("Cannot write isolated fixture");
            return 0;
        }
        if(argc==4&&std::string(argv[1])=="--verify") {
            SqliteConnection db(argv[2]);Json::Value receipts;std::ifstream input(argv[3]);input>>receipts;
            for(const auto *name:{"reporter","worker"}) {
                const auto id=receipts[name]["id"].asInt64();
                auto q=db.prepare("SELECT o.reporter_id,o.status,o.version,o.order_no,o.submitted_at,u.username "
                    "FROM work_orders o JOIN users u ON u.id=o.reporter_id WHERE o.id=?1");q.bindInt(1,id);
                if(!q.step()||q.columnText(1)!="PENDING_REVIEW"||q.columnInt(2)!=2||q.columnText(3).size()!=14||q.columnIsNull(4)||
                   q.columnText(5)!=(std::string(name)=="reporter"?"demo_reporter01":"demo_worker01")) throw std::runtime_error("HTTP row verification failed");
                const auto actor=q.columnInt(0);
                auto logs=db.prepare("SELECT action,actor_id,from_status,to_status,request_id FROM status_logs WHERE order_id=?1 ORDER BY id");
                logs.bindInt(1,id);
                if(!logs.step()||logs.columnText(0)!="ORDER_CREATED"||logs.columnInt(1)!=actor||!logs.columnIsNull(2)||
                    logs.columnText(3)!="DRAFT"||logs.columnText(4)!=receipts[name]["create_request_id"].asString()) throw std::runtime_error("HTTP create log mismatch");
                if(!logs.step()||logs.columnText(0)!="ORDER_SUBMITTED"||logs.columnInt(1)!=actor||logs.columnText(2)!="DRAFT"||
                    logs.columnText(3)!="PENDING_REVIEW"||logs.columnText(4)!=receipts[name]["submit_request_id"].asString()||logs.step()) throw std::runtime_error("HTTP submit log mismatch");
                std::cout<<"PASS HTTP database order="<<id<<" actor="<<actor<<" status=PENDING_REVIEW version=2 number="<<q.columnText(3)<<" events=2\n";
            }
            if(db.scalarInt("SELECT count(*) FROM work_orders")!=6||db.scalarInt("SELECT sum(last_value) FROM order_number_sequences")!=2)
                throw std::runtime_error("Failed HTTP requests left extra orders or sequences");
            if(db.scalarInt("SELECT count(*) FROM pragma_foreign_key_check")!=0) throw std::runtime_error("Foreign key violation");
            auto integrity=db.prepare("PRAGMA integrity_check");
            if(!integrity.step()||integrity.columnText(0)!="ok") throw std::runtime_error("Integrity failed");
            std::cout<<"PASS HTTP database integrity, no failed-request writes or extra events\n";return 0;
        }
        return 2;
    }catch(const std::exception &e) {std::cerr<<e.what()<<'\n';return 1;}
}
