#include "ReviewTestData.h"
#include <json/json.h>
#include <fstream>
#include <iostream>
using namespace campusfix;
int main(int argc,char **argv) {
    try {
        if(argc==6&&std::string(argv[1])=="--init") {
            const auto f=test::readFixture(argv[2],argv[4],argv[5]);Json::Value data;
            for(const auto &a:f.accounts)data[a.first]=a.second.token;
            SqliteConnection db(argv[2]);
            for(const auto *mode:{"expired","revoked"}) {
                const auto token=newToken();data[mode]=token;AuthRepository(db).createSession(newOpaqueId(),f.accounts.at("demo_reporter01").id,tokenDigest(token),std::string(mode)=="expired"?"2020-01-01T00:00:00.000Z":"2099-01-01T00:00:00.000Z");
                if(std::string(mode)=="revoked"){auto q=db.prepare("UPDATE auth_sessions SET revoked_at='2020-01-01T00:00:00.000Z' WHERE token_hash=?1");q.bindText(1,tokenDigest(token));q.step();}
            }
            data["assigned"]=Json::Int64(f.current);data["other"]=Json::Int64(f.foreign);
            std::ofstream out(argv[3]);out<<data;return out?0:1;
        }
        SqliteConnection db(argv[2]);const std::string op=argv[1];
        if(op=="--snapshot") {std::cout<<test::reviewFingerprint(db);return 0;}
        if(op=="--verify") {test::integrity(db);std::cout<<"integrity_check=ok; foreign_key_check empty";return 0;}
        if(op=="--inspect") {
            auto q=db.prepare("SELECT id,reporter_id,status,version,category_id,location_id,title,description,requested_urgency,approved_urgency,contact_name,contact_phone,expected_visit_at,order_no,submitted_at,created_at,updated_at,cancelled_at FROM work_orders WHERE id=?1");q.bindInt(1,std::stoll(argv[3]));if(!q.step())return 1;
            const char *keys[]={"id","reporter_id","status","version","category_id","location_id","title","description","requested_urgency","approved_urgency","contact_name","contact_phone","expected_visit_at","order_no","submitted_at","created_at","updated_at","cancelled_at"};Json::Value data;
            for(int i=0;i<18;i++) {if(q.columnIsNull(i))data[keys[i]]=Json::nullValue;else if(i==0||i==1||i==3||i==4||i==5)data[keys[i]]=Json::Int64(q.columnInt(i));else data[keys[i]]=q.columnText(i);}
            data["sequence"]=Json::Int64(db.scalarInt("SELECT coalesce(sum(last_value),0) FROM order_number_sequences"));data["orders"]=Json::Int64(db.scalarInt("SELECT count(*) FROM work_orders"));data["logs"]=Json::arrayValue;
            auto logs=db.prepare("SELECT action,actor_id,from_status,to_status,public_note,metadata_json,request_id,created_at FROM status_logs WHERE order_id=?1 ORDER BY id");logs.bindInt(1,std::stoll(argv[3]));
            const char *names[]={"action","actor_id","from_status","to_status","public_note","metadata_json","request_id","created_at"};while(logs.step()){Json::Value row;for(int i=0;i<8;i++){if(logs.columnIsNull(i))row[names[i]]=Json::nullValue;else if(i==1)row[names[i]]=Json::Int64(logs.columnInt(i));else row[names[i]]=logs.columnText(i);}data["logs"].append(row);}
            std::cout<<data;return 0;
        }
        if(op=="--mode") {
            const std::string mode=argv[3];
            if(mode=="log-failure")db.execute("CREATE TRIGGER fail_edit BEFORE INSERT ON status_logs WHEN NEW.action='ORDER_CONTENT_UPDATED' BEGIN SELECT RAISE(ABORT,'test fault'); END");
            else if(mode=="update-failure")db.execute("CREATE TRIGGER fail_edit AFTER UPDATE OF description ON work_orders BEGIN SELECT RAISE(ABORT,'test fault'); END");
            else if(mode=="submit-failure")db.execute("CREATE TRIGGER fail_edit BEFORE INSERT ON status_logs WHEN NEW.action='ORDER_SUBMITTED' BEGIN SELECT RAISE(ABORT,'test fault'); END");
            else if(mode=="clear")db.execute("DROP TRIGGER fail_edit");
            else if(mode=="disable-location")db.execute("UPDATE locations SET active=0 WHERE id=11");
            else if(mode=="enable-location")db.execute("UPDATE locations SET active=1 WHERE id=11");
            else if(mode=="disable-category")db.execute("UPDATE fault_categories SET active=0 WHERE id=1");
            else if(mode=="enable-category")db.execute("UPDATE fault_categories SET active=1 WHERE id=1");
            else if(mode=="disable-ancestor")db.execute("UPDATE locations SET active=0 WHERE id=1");
            else if(mode=="enable-ancestor")db.execute("UPDATE locations SET active=1 WHERE id=1");
            else throw std::runtime_error("Unknown fixture mode");return 0;
        }
        return 2;
    }catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}
}
