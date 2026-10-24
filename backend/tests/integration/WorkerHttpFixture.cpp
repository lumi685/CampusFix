#include "WorkerTestData.h"
#include <json/json.h>
#include <fstream>
#include <iostream>
using namespace campusfix;
int main(int argc,char **argv) {
    try {
        if(argc==6&&std::string(argv[1])=="--init") {
            const auto f=test::workerFixture(argv[2],argv[4],argv[5]);SqliteConnection db(argv[2]);Json::Value data;
            for(const auto &a:f.accounts)data[a.first]=a.second.token;
            for(const auto *mode:{"expired","revoked"}) {
                const auto token=newToken();data[mode]=token;AuthRepository(db).createSession(newOpaqueId(),f.accounts.at("demo_dispatcher").id,tokenDigest(token),std::string(mode)=="expired"?"2020-01-01T00:00:00.000Z":"2099-01-01T00:00:00.000Z");
                if(std::string(mode)=="revoked"){auto q=db.prepare("UPDATE auth_sessions SET revoked_at='2020-01-01T00:00:00.000Z' WHERE token_hash=?1");q.bindText(1,tokenDigest(token));q.step();}
            }
            data["business_fingerprint"]=test::reviewFingerprint(db);std::ofstream out(argv[3]);out<<data;return out?0:1;
        }
        if(argc<3)return 2;SqliteConnection db(argv[2]);const std::string op=argv[1];
        if(op=="--snapshot"){std::cout<<test::workerFingerprint(db);return 0;}
        if(op=="--inspect") {
            auto q=db.prepare("SELECT w.id,w.user_id,w.worker_no,w.service_area_id,w.availability,w.max_concurrent_tasks,w.emergency_duty,w.active,w.updated_at,u.role,u.status "
                "FROM workers w JOIN users u ON u.id=w.user_id WHERE w.id=?1");q.bindInt(1,std::stoll(argv[3]));if(!q.step())return 1;
            Json::Value data;const char *keys[]={"id","user_id","worker_no","service_area_id","availability","max_concurrent_tasks","emergency_duty","active","updated_at","account_role","account_status"};
            for(int n=0;n<11;n++){if(n==0||n==1||n==3||n==5)data[keys[n]]=Json::Int64(q.columnInt(n));else if(n==6||n==7)data[keys[n]]=q.columnInt(n)!=0;else data[keys[n]]=q.columnText(n);}
            data["skills"]=Json::arrayValue;auto s=db.prepare("SELECT ws.skill_id,ws.level,ws.certified,ws.active,ws.effective_at,ws.updated_at,s.requires_certificate,s.active "
                "FROM worker_skills ws JOIN skills s ON s.id=ws.skill_id WHERE ws.worker_id=?1 ORDER BY skill_id");s.bindInt(1,std::stoll(argv[3]));
            while(s.step()){Json::Value row;row["skill_id"]=Json::Int64(s.columnInt(0));row["level"]=Json::Int64(s.columnInt(1));row["certified"]=s.columnInt(2)!=0;row["active"]=s.columnInt(3)!=0;row["effective_at"]=s.columnText(4);row["updated_at"]=s.columnText(5);row["requires_certificate"]=s.columnInt(6)!=0;row["skill_active"]=s.columnInt(7)!=0;data["skills"].append(row);}
            data["active_tasks"]=Json::Int64(WorkerRepository(db).find(std::stoll(argv[3])).activeTasks);std::cout<<data;return 0;
        }
        if(op=="--verify") {
            std::ifstream in(argv[3]);Json::Value data;in>>data;
            if(test::reviewFingerprint(db)!=data["business_fingerprint"].asString())throw std::runtime_error("Worker API changed work orders/assignments/audit");
            test::integrity(db);std::cout<<"integrity_check=ok; foreign_key_check empty; orders/assignments/audit unchanged";return 0;
        }
        if(op=="--mode") {
            const std::string mode=argv[3];SqliteTransaction tx(db);
            if(mode=="admin-area")test::grant(db,"demo_sysadmin","AREA_SCOPE",1);
            else if(mode=="admin-worker-b")test::grant(db,"demo_sysadmin","WORKER_MANAGE",2);
            else if(mode=="admin-worker-a")test::grant(db,"demo_sysadmin","WORKER_MANAGE",1);
            else if(mode=="admin-global"){test::revoke(db,"demo_sysadmin","WORKER_MANAGE");test::grant(db,"demo_sysadmin","WORKER_MANAGE",{});}
            else if(mode=="revoke-admin")test::revoke(db,"demo_sysadmin","WORKER_MANAGE");
            else if(mode=="overlap-a")test::grant(db,"demo_dispatcher","AREA_SCOPE",3);
            else if(mode=="revoke-a")test::revoke(db,"demo_dispatcher","AREA_SCOPE");
            else if(mode=="restore-a")test::grant(db,"demo_dispatcher","AREA_SCOPE",1);
            else if(mode=="disable-user")db.execute("UPDATE users SET status='DISABLED' WHERE username='demo_dispatcher'");
            else if(mode=="enable-user")db.execute("UPDATE users SET status='ACTIVE' WHERE username='demo_dispatcher'");
            else if(mode=="disable-skill")db.execute("UPDATE skills SET active=0 WHERE id=1");
            else if(mode=="enable-skill")db.execute("UPDATE skills SET active=1 WHERE id=1");
            else if(mode=="disable-area")db.execute("UPDATE locations SET active=0 WHERE id=3");
            else if(mode=="enable-area")db.execute("UPDATE locations SET active=1 WHERE id=3");
            else if(mode=="overload")db.execute("UPDATE workers SET max_concurrent_tasks=1 WHERE id=1");
            else if(mode=="restore-capacity")db.execute("UPDATE workers SET max_concurrent_tasks=3 WHERE id=1");
            else if(mode=="fail-profile")db.execute("CREATE TRIGGER fail_worker AFTER UPDATE ON workers BEGIN SELECT RAISE(ABORT,'fixture worker failure'); END");
            else if(mode=="fail-skill")db.execute("CREATE TRIGGER fail_worker BEFORE INSERT ON worker_skills WHEN NEW.skill_id=3 BEGIN SELECT RAISE(ABORT,'fixture second skill failure'); END");
            else if(mode=="clear-failure")db.execute("DROP TRIGGER fail_worker");
            else throw std::runtime_error("Unknown worker fixture mode");tx.commit();return 0;
        }
        return 2;
    }catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}
}
