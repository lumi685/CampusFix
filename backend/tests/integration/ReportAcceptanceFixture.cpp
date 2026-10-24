// Isolated test executable; never linked into the production HTTP server.
#include "WorkOrderReadTestData.h"
#include "services/DemoAccountActivation.h"
#include <json/json.h>
#include <fstream>
#include <iostream>
#include <memory>

using namespace campusfix;
namespace {
void save(const char *file,const Json::Value &value) {
    std::ofstream out(file,std::ios::binary);out<<value;
    if(!out) throw std::runtime_error("Cannot write temporary test artifact");
}
Json::Value selectRows(SqliteConnection &db,const char *sql) {
    sqlite3_stmt *raw=nullptr;
    if(sqlite3_prepare_v2(db.nativeHandle(),sql,-1,&raw,nullptr)!=SQLITE_OK) throw std::runtime_error("Cannot prepare evidence SELECT");
    const std::unique_ptr<sqlite3_stmt,decltype(&sqlite3_finalize)> statement(raw,&sqlite3_finalize);
    Json::Value result(Json::arrayValue);int status=SQLITE_OK;
    while((status=sqlite3_step(raw))==SQLITE_ROW) {
        Json::Value row(Json::objectValue);
        for(int i=0;i<sqlite3_column_count(raw);++i) {
            const auto key=sqlite3_column_name(raw,i);
            if(sqlite3_column_type(raw,i)==SQLITE_NULL) row[key]=Json::nullValue;
            else if(sqlite3_column_type(raw,i)==SQLITE_INTEGER) row[key]=Json::Int64(sqlite3_column_int64(raw,i));
            else row[key]=reinterpret_cast<const char *>(sqlite3_column_text(raw,i));
        }
        result.append(row);
    }
    if(status!=SQLITE_DONE) throw std::runtime_error("Cannot read SQLite evidence");
    return result;
}
Json::Value snapshot(const std::filesystem::path &path) {
    SqliteConnection db(path);SqliteTransaction tx(db,false);Json::Value result(Json::objectValue);
    result["orders"]=selectRows(db,"SELECT id,reporter_id,category_id,location_id,title,description,contact_name,contact_phone,requested_urgency,expected_visit_at,status,version,order_no,created_at,submitted_at,updated_at FROM work_orders ORDER BY id");
    result["logs"]=selectRows(db,"SELECT id,order_id,actor_id,action,from_status,to_status,created_at,request_id FROM status_logs ORDER BY id");
    result["sequence_sum"]=Json::Int64(db.scalarInt("SELECT coalesce(sum(last_value),0) FROM order_number_sequences"));
    result["foreign_key_violations"]=Json::Int64(db.scalarInt("SELECT count(*) FROM pragma_foreign_key_check"));
    auto integrity=db.prepare("PRAGMA integrity_check");
    if(!integrity.step()) throw std::runtime_error("Cannot check integrity");result["integrity"]=integrity.columnText(0);
    sqlite3_int64 size=0;auto *bytes=sqlite3_serialize(db.nativeHandle(),"main",&size,0);
    if(!bytes) throw std::runtime_error("Cannot fingerprint SQLite");
    result["digest"]=tokenDigest(std::string(reinterpret_cast<char *>(bytes),static_cast<std::size_t>(size)));sqlite3_free(bytes);
    tx.commit();return result;
}
}
int main(int argc,char **argv) {
    try {
        initializeCrypto();
        if(argc==6&&std::string(argv[1])=="--init") {
            if(std::filesystem::exists(argv[2])) throw std::runtime_error("Refuse to overwrite an existing test database");
            const auto fixture=test::readFixture(argv[2],argv[4],argv[5]);Json::Value data(Json::objectValue);
            for(const auto *name:{"demo_reporter01","demo_reporter02","demo_reporter03","demo_worker01","demo_dispatcher","demo_sysadmin"}) {
                {
                    SqliteConnection db(argv[2]);auto disable=db.prepare("UPDATE users SET status='DISABLED' WHERE username=?1 AND password_hash='!UNSET_DEMO_CREDENTIAL!'");
                    disable.bindText(1,name);disable.step();
                }
                const auto password=newToken();setDemoPassword(argv[2],name,password);enableDemoAccount(argv[2],name);
                SqliteConnection db(argv[2]);const auto user=AuthRepository(db).userByName(name);
                data["accounts"][name]["password"]=password;data["accounts"][name]["id"]=Json::Int64(user.id);data["accounts"][name]["role"]=user.role;
            }
            SqliteConnection db(argv[2]);
            db.execute("INSERT INTO locations(id,code,name,location_type,active) VALUES(1001,'ACCEPTANCE-INACTIVE','Fictional inactive location','CAMPUS',0)");
            db.execute("INSERT INTO fault_categories(id,code,name,required_skill_id,target_completion_minutes,active) VALUES(1001,'ACCEPTANCE-INACTIVE','Fictional inactive category',1,60,0)");
            data["inactive_location"]=1001;data["inactive_category"]=1001;
            data["current_assignment_order"]=Json::Int64(fixture.current);data["other_campus_order"]=Json::Int64(fixture.foreign);
            data["baseline_orders"]=Json::Int64(db.scalarInt("SELECT count(*) FROM work_orders"));
            save(argv[3],data);std::cout<<"PASS isolated acceptance fixture; random credentials written only to temporary file\n";return 0;
        }
        if(argc==4&&std::string(argv[1])=="--snapshot") {save(argv[3],snapshot(argv[2]));return 0;}
        if(argc==4&&std::string(argv[1])=="--fault") {
            SqliteConnection db(argv[2]);db.execute("DROP TRIGGER IF EXISTS acceptance_fault");
            const std::string mode=argv[3];
            if(mode=="create-log") db.execute("CREATE TRIGGER acceptance_fault BEFORE INSERT ON status_logs WHEN NEW.action='ORDER_CREATED' BEGIN SELECT RAISE(ABORT,'acceptance injected failure'); END");
            else if(mode=="submit-log") db.execute("CREATE TRIGGER acceptance_fault BEFORE INSERT ON status_logs WHEN NEW.action='ORDER_SUBMITTED' BEGIN SELECT RAISE(ABORT,'acceptance injected failure'); END");
            else if(mode!="off") throw std::runtime_error("Unknown fault mode");
            return 0;
        }
        if(argc==4&&std::string(argv[1])=="--grant") {SqliteConnection db(argv[2]);test::readGrantMode(db,argv[3]);return 0;}
        return 2;
    }catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}
}
