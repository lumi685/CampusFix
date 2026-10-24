#include "ReviewTestData.h"
#include <json/json.h>
#include <fstream>
#include <iostream>

using namespace campusfix;
int main(int argc,char **argv) {
    try {
        if(argc==6&&std::string(argv[1])=="--init") {
            const auto f=test::readFixture(argv[2],argv[4],argv[5]);SqliteConnection db(argv[2]);Json::Value data(Json::objectValue);
            for(const auto &a:f.accounts) data[a.first]=a.second.token;
            for(const auto *key:{"approve","return","reject","validation","race","log_failure","update_failure","reject_failure","admin"})
                data[key]=Json::Int64(test::pending(db,f).id);
            data["worker"]=Json::Int64(test::pending(db,f,"demo_worker01").id);
            data["foreign"]=Json::Int64(test::pending(db,f,"demo_reporter02",13).id);
            data["current"]=Json::Int64(f.current);data["draft"]=Json::Int64(f.draft);
            for(const auto *mode:{"expired","revoked"}) {
                const auto token=newToken();data[mode]=token;
                AuthRepository(db).createSession(newOpaqueId(),f.accounts.at("demo_dispatcher").id,tokenDigest(token),
                    std::string(mode)=="expired"?"2020-01-01T00:00:00.000Z":"2099-01-01T00:00:00.000Z");
                if(std::string(mode)=="revoked") {auto q=db.prepare("UPDATE auth_sessions SET revoked_at='2020-01-01T00:00:00.000Z' WHERE token_hash=?1");q.bindText(1,tokenDigest(token));q.step();}
            }
            std::ofstream output(argv[3],std::ios::binary);output<<data;if(!output) throw std::runtime_error("Fixture output failed");return 0;
        }
        if(argc==3&&std::string(argv[1])=="--snapshot") {SqliteConnection db(argv[2]);std::cout<<test::reviewFingerprint(db);return 0;}
        if(argc==3&&std::string(argv[1])=="--integrity") {SqliteConnection db(argv[2]);test::integrity(db);std::cout<<"integrity_check=ok; foreign_key_check empty\n";return 0;}
        if(argc==4&&std::string(argv[1])=="--inspect") {
            SqliteConnection db(argv[2]);const auto id=std::stoll(argv[3]);Json::Value data(Json::objectValue);
            auto q=db.prepare("SELECT id,status,version,category_id,requested_urgency,approved_urgency,order_no,submitted_at,cancelled_at,updated_at FROM work_orders WHERE id=?1");q.bindInt(1,id);
            if(!q.step()) throw std::runtime_error("Fixture order missing");
            const char *names[]={"id","status","version","category_id","requested_urgency","approved_urgency","order_no","submitted_at","cancelled_at","updated_at"};
            for(int i=0;i<10;++i) {if(q.columnIsNull(i)) data[names[i]]=Json::nullValue;else if(i==0||i==2||i==3) data[names[i]]=Json::Int64(q.columnInt(i));else data[names[i]]=q.columnText(i);}
            data["logs"]=Json::Value(Json::arrayValue);
            auto logs=db.prepare("SELECT actor_id,action,from_status,to_status,public_note,internal_note,metadata_json,created_at,request_id FROM status_logs WHERE order_id=?1 ORDER BY id");logs.bindInt(1,id);
            const char *keys[]={"actor_id","action","from_status","to_status","public_note","internal_note","metadata_json","created_at","request_id"};
            while(logs.step()) {Json::Value row(Json::objectValue);for(int i=0;i<9;++i) {if(logs.columnIsNull(i)) row[keys[i]]=Json::nullValue;else if(i==0) row[keys[i]]=Json::Int64(logs.columnInt(i));else row[keys[i]]=logs.columnText(i);}data["logs"].append(row);}
            std::cout<<data;return 0;
        }
        if(argc==4&&std::string(argv[1])=="--mode") {
            SqliteConnection db(argv[2]);SqliteTransaction tx(db);const std::string mode=argv[3];
            if(mode=="admin-read") test::grant(db,"demo_sysadmin","BUSINESS_READ",1);
            else if(mode=="admin-area") test::grant(db,"demo_sysadmin","AREA_SCOPE",1);
            else if(mode=="admin-wrong-review") test::grant(db,"demo_sysadmin","REVIEW",2);
            else if(mode=="admin-review") {test::revoke(db,"demo_sysadmin","REVIEW");test::grant(db,"demo_sysadmin","REVIEW",3);}
            else if(mode=="admin-global") {test::revoke(db,"demo_sysadmin","REVIEW");test::grant(db,"demo_sysadmin","REVIEW",{});}
            else if(mode=="revoke-review") test::revoke(db,"demo_sysadmin","REVIEW");
            else if(mode=="admin-internal") test::grant(db,"demo_sysadmin","INTERNAL_NOTE_READ",11);
            else if(mode=="disable-dispatcher") db.execute("UPDATE users SET status='DISABLED' WHERE username='demo_dispatcher'");
            else if(mode=="enable-dispatcher") db.execute("UPDATE users SET status='ACTIVE' WHERE username='demo_dispatcher'");
            else if(mode=="revoke-dispatcher") test::revoke(db,"demo_dispatcher","AREA_SCOPE");
            else if(mode=="restore-dispatcher") test::grant(db,"demo_dispatcher","AREA_SCOPE",1);
            else if(mode=="disable-category") db.execute("UPDATE fault_categories SET active=0 WHERE id=2");
            else if(mode=="enable-category") db.execute("UPDATE fault_categories SET active=1 WHERE id=2");
            else if(mode=="log-failure") db.execute("CREATE TRIGGER fail_review BEFORE INSERT ON status_logs WHEN NEW.action IN ('ORDER_REVIEWED','ORDER_RETURNED','ORDER_REJECTED') BEGIN SELECT RAISE(ABORT,'fixture failure'); END");
            else if(mode=="update-failure") db.execute("CREATE TRIGGER fail_review BEFORE UPDATE OF category_id,approved_urgency ON work_orders BEGIN SELECT RAISE(ABORT,'fixture failure'); END");
            else if(mode=="reject-failure") db.execute("CREATE TRIGGER fail_review AFTER UPDATE OF cancelled_at ON work_orders BEGIN SELECT RAISE(ABORT,'fixture failure'); END");
            else if(mode=="clear-failure") db.execute("DROP TRIGGER fail_review");
            else throw std::runtime_error("Unknown fixture mode");
            tx.commit();return 0;
        }
        return 2;
    }catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}
}
