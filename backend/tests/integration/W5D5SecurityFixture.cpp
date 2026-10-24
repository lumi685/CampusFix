#include "WorkerTestData.h"
#include <json/json.h>
#include <fstream>
#include <iostream>
#include <sstream>
#include <thread>
#include <memory>

using namespace campusfix;
namespace {
using Statement=std::unique_ptr<sqlite3_stmt,decltype(&sqlite3_finalize)>;
Statement statement(SqliteConnection &db,const std::string &sql) {
    sqlite3_stmt *raw=nullptr;const auto rc=sqlite3_prepare_v2(db.nativeHandle(),sql.c_str(),-1,&raw,nullptr);
    if(rc!=SQLITE_OK)throw SqliteError(rc,"Test snapshot prepare failed");return Statement(raw,sqlite3_finalize);
}
bool step(sqlite3_stmt *q) {const auto rc=sqlite3_step(q);if(rc!=SQLITE_ROW&&rc!=SQLITE_DONE)throw SqliteError(rc,"Test snapshot read failed");return rc==SQLITE_ROW;}
std::string text(sqlite3_stmt *q,int n) {return std::string(reinterpret_cast<const char *>(sqlite3_column_text(q,n)),sqlite3_column_bytes(q,n));}
std::string digestRows(SqliteConnection &db,const std::string &sql) {
    auto q=statement(db,sql);std::ostringstream out;
    const auto count=sqlite3_column_count(q.get());
    while(step(q.get()))for(int n=0;n<count;n++) {
        if(sqlite3_column_type(q.get(),n)==SQLITE_NULL)out<<"N;";else {const auto s=text(q.get(),n);out<<s.size()<<':'<<s<<';';}
    }
    return tokenDigest(out.str());
}
Json::Value rows(SqliteConnection &db,const std::string &sql,std::int64_t id) {
    auto q=statement(db,sql);if(sqlite3_bind_int64(q.get(),1,id)!=SQLITE_OK)throw std::runtime_error("Test snapshot bind failed");Json::Value result(Json::arrayValue);
    while(step(q.get())) {
        Json::Value row(Json::objectValue);const int count=sqlite3_column_count(q.get());
        for(int n=0;n<count;n++) {
            const auto key=sqlite3_column_name(q.get(),n);
            if(sqlite3_column_type(q.get(),n)==SQLITE_NULL)row[key]=Json::nullValue;
            else if(sqlite3_column_type(q.get(),n)==SQLITE_INTEGER)row[key]=Json::Int64(sqlite3_column_int64(q.get(),n));
            else row[key]=text(q.get(),n);
        }
        result.append(row);
    }
    return result;
}
}
int main(int argc,char **argv) {
    try {
        initializeCrypto();
        if(argc==6&&std::string(argv[1])=="--init") {
            auto f=test::workerFixture(argv[2],argv[4],argv[5]);SqliteConnection db(argv[2]);Json::Value data;
            // Account creation is test-only; placeholder hashes are not usable passwords.
            for(const auto *name:{"test_admin_r","test_admin_rb","test_admin_b","test_admin_w","test_admin_mix","test_admin_area","test_admin_global"}) {
                auto q=db.prepare("INSERT INTO users(username,password_hash,display_name,role,status) VALUES(?1,'!UNSET_DEMO_CREDENTIAL!',?1,'sysadmin','ACTIVE')");q.bindText(1,name);q.step();
                const auto user=AuthRepository(db).userByName(name);const auto token=newToken();AuthRepository(db).createSession(newOpaqueId(),user.id,tokenDigest(token),"2099-01-01T00:00:00.000Z");f.accounts[name]={user.id,token};
            }
            db.execute("UPDATE users SET status='ACTIVE' WHERE username='demo_worker04'");
            const auto b=AuthRepository(db).userByName("demo_worker04");const auto bt=newToken();AuthRepository(db).createSession(newOpaqueId(),b.id,tokenDigest(bt),"2099-01-01T00:00:00.000Z");f.accounts["demo_worker04"]={b.id,bt};
            test::grant(db,"test_admin_r","REVIEW",1);test::grant(db,"test_admin_rb","REVIEW",2);
            test::grant(db,"test_admin_b","BUSINESS_READ",2);test::grant(db,"test_admin_w","WORKER_MANAGE",2);
            test::grant(db,"test_admin_area","AREA_SCOPE",1);test::grant(db,"test_admin_global","BUSINESS_READ",{});
            test::grant(db,"test_admin_mix","BUSINESS_READ",2);test::grant(db,"test_admin_mix","REVIEW",1);
            test::grant(db,"test_admin_mix","WORKER_MANAGE",2);test::grant(db,"test_admin_mix","AREA_SCOPE",1);
            for(const auto &a:f.accounts) {data["tokens"][a.first]=a.second.token;data["user_ids"][a.first]=Json::Int64(a.second.id);}
            for(const auto *key:{"approve","returned","reject","validation","review_r","review_mix","revoke","dispatch","cancelled","supplement"}) {
                auto o=test::pending(db,f);data["orders"][key]=Json::Int64(o.id);
                if(std::string(key)=="dispatch")WorkOrderService(db).review(f.accounts.at("demo_dispatcher").id,f.accounts.at("demo_dispatcher").token,o.id,test::approve(),"fixture-approved");
                if(std::string(key)=="cancelled")WorkOrderService(db).review(f.accounts.at("demo_dispatcher").id,f.accounts.at("demo_dispatcher").token,o.id,test::outcome("REJECT"),"fixture-rejected");
                if(std::string(key)=="supplement")WorkOrderService(db).review(f.accounts.at("demo_dispatcher").id,f.accounts.at("demo_dispatcher").token,o.id,test::outcome("RETURN"),"fixture-returned");
            }
            data["orders"]["b"]=Json::Int64(test::pending(db,f,"demo_reporter02",13).id);
            data["orders"]["draft"]=Json::Int64(f.draft);
            for(const auto *key:{"expired","revoked"}) {
                const auto t=newToken();AuthRepository(db).createSession(newOpaqueId(),f.accounts.at("demo_dispatcher").id,tokenDigest(t),std::string(key)=="expired"?"2020-01-01T00:00:00.000Z":"2099-01-01T00:00:00.000Z");
                if(std::string(key)=="revoked")AuthRepository(db).revokeSession(tokenDigest(t));data["tokens"][key]=t;
            }
            std::ofstream out(argv[3],std::ios::binary);out<<data;return out?0:1;
        }
        if(argc<3)return 2;SqliteConnection db(argv[2]);const std::string op=argv[1];
        if(op=="--snapshot") {
            Json::Value j;j["all"]=test::workerFingerprint(db);j["business"]=test::reviewFingerprint(db);
            j["workers"]=digestRows(db,"SELECT * FROM workers ORDER BY id")+digestRows(db,"SELECT * FROM worker_skills ORDER BY worker_id,skill_id");
            j["accounts"]=digestRows(db,"SELECT id,role,status FROM users ORDER BY id");
            j["orders"]=Json::Int64(db.scalarInt("SELECT count(*) FROM work_orders"));j["logs"]=Json::Int64(db.scalarInt("SELECT count(*) FROM status_logs"));std::cout<<j;return 0;
        }
        if(op=="--order"&&argc==4) {
            Json::Value j;const auto id=std::stoll(argv[3]);j["row"]=rows(db,"SELECT id,reporter_id,order_no,status,version,category_id,approved_urgency,created_at,submitted_at,updated_at,cancelled_at FROM work_orders WHERE id=?1",id)[0];
            j["logs"]=rows(db,"SELECT id,actor_id,action,from_status,to_status,public_note,internal_note,created_at,request_id FROM status_logs WHERE order_id=?1 ORDER BY id",id);std::cout<<j;return 0;
        }
        if(op=="--worker"&&argc==4) {
            const auto id=std::stoll(argv[3]);Json::Value j;
            j["row"]=rows(db,"SELECT w.*,u.role AS account_role,u.status AS account_status,(SELECT count(*) FROM assignments a WHERE a.worker_id=w.id AND a.ended_at IS NULL) AS active_tasks FROM workers w JOIN users u ON u.id=w.user_id WHERE w.id=?1",id)[0];
            j["skills"]=rows(db,"SELECT * FROM worker_skills WHERE worker_id=?1 ORDER BY skill_id",id);
            j["others"]=digestRows(db,"SELECT * FROM workers WHERE id<>"+std::to_string(id)+" ORDER BY id")+digestRows(db,"SELECT * FROM worker_skills WHERE worker_id<>"+std::to_string(id)+" ORDER BY worker_id,skill_id");std::cout<<j;return 0;
        }
        if(op=="--integrity") {test::integrity(db);std::cout<<"integrity_check=ok; foreign_key_check empty";return 0;}
        if(op=="--hold"&&argc==5) {
            SqliteTransaction tx(db);{std::ofstream ready(argv[3]);ready<<"locked";}
            for(int i=0;i<1000;i++){if(std::filesystem::exists(argv[4])){tx.commit();return 0;}std::this_thread::sleep_for(std::chrono::milliseconds(10));}
            throw std::runtime_error("Concurrency gate release timed out");
        }
        if(op=="--mode"&&argc==4) {
            SqliteTransaction tx(db);const std::string mode=argv[3];
            if(mode=="overlap") {test::grant(db,"demo_dispatcher","AREA_SCOPE",3);test::grant(db,"test_admin_global","BUSINESS_READ",1);test::grant(db,"test_admin_global","BUSINESS_READ",3);}
            else if(mode=="revoke-r")test::revoke(db,"test_admin_r","REVIEW");
            else if(mode=="restore-r")test::grant(db,"test_admin_r","REVIEW",1);
            else if(mode=="disable-da")db.execute("UPDATE users SET status='DISABLED' WHERE username='demo_dispatcher'");
            else if(mode=="enable-da")db.execute("UPDATE users SET status='ACTIVE' WHERE username='demo_dispatcher'");
            else if(mode=="mix-wm-a")test::grant(db,"test_admin_mix","WORKER_MANAGE",1);
            else if(mode=="mix-wm-b")test::grant(db,"test_admin_mix","WORKER_MANAGE",2);
            else if(mode=="mix-revoke-a"||mode=="mix-revoke-b") {
                auto q=db.prepare("UPDATE management_grants SET revoked_at='2020-01-01T00:00:00.000Z' WHERE user_id=(SELECT id FROM users WHERE username='test_admin_mix') AND capability='WORKER_MANAGE' AND scope_location_id=?1 AND revoked_at IS NULL");q.bindInt(1,mode=="mix-revoke-a"?1:2);q.step();
            }
            else if(mode=="off-category")db.execute("UPDATE fault_categories SET active=0 WHERE id=2");
            else if(mode=="on-category")db.execute("UPDATE fault_categories SET active=1 WHERE id=2");
            else if(mode=="off-skill")db.execute("UPDATE skills SET active=0 WHERE id=1");
            else if(mode=="on-skill")db.execute("UPDATE skills SET active=1 WHERE id=1");
            else if(mode=="off-area")db.execute("UPDATE locations SET active=0 WHERE id=3");
            else if(mode=="on-area")db.execute("UPDATE locations SET active=1 WHERE id=3");
            else if(mode=="fail-skill")db.execute("CREATE TRIGGER w5d5_fail BEFORE INSERT ON worker_skills WHEN NEW.skill_id=3 BEGIN SELECT RAISE(ABORT,'test skill write failure'); END");
            else if(mode=="fail-log")db.execute("CREATE TRIGGER w5d5_fail BEFORE INSERT ON status_logs BEGIN SELECT RAISE(ABORT,'test audit failure'); END");
            else if(mode=="clear-fault")db.execute("DROP TRIGGER w5d5_fail");
            else throw std::runtime_error("Unknown fixture mode");tx.commit();return 0;
        }
        return 2;
    }catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}
}
