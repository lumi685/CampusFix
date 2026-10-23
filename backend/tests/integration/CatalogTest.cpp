#include "WorkOrderReadTestData.h"
#include "services/CatalogService.h"
#include <json/json.h>
#include <fstream>
#include <iostream>

using namespace campusfix;
namespace {
int checks=0;
void check(bool ok) {++checks;if(!ok) throw std::runtime_error("Catalog assertion failed #"+std::to_string(checks));}
std::string fingerprint(SqliteConnection &db) {
    sqlite3_int64 n=0;auto *bytes=sqlite3_serialize(db.nativeHandle(),"main",&n,0);
    if(!bytes) throw std::runtime_error("Cannot snapshot database");
    const auto result=tokenDigest(std::string(reinterpret_cast<char *>(bytes),static_cast<std::size_t>(n)));
    sqlite3_free(bytes);return result;
}
Json::Value init(const std::filesystem::path &path,const std::filesystem::path &migrations,const std::filesystem::path &seed) {
    const auto f=test::readFixture(path,migrations,seed);SqliteConnection db(path);
    db.execute("INSERT INTO locations(id,parent_id,code,name,location_type,active) VALUES"
        "(101,NULL,'TEST-DISABLED','Disabled root','CAMPUS',0),(102,101,'TEST-HIDDEN-CHILD','Hidden child','BUILDING',1),"
        "(103,102,'TEST-HIDDEN-GRANDCHILD','Hidden grandchild','FLOOR',1),(104,1,'TEST-DISABLED-CHILD','Disabled child','BUILDING',0)");
    db.execute("INSERT INTO fault_categories(id,parent_id,code,name,required_skill_id,target_completion_minutes,active) VALUES"
        "(101,NULL,'TEST-CATEGORY','Category root',1,60,1),(102,101,'TEST-CHILD','Category child',1,60,1),"
        "(103,102,'TEST-GRANDCHILD','Category grandchild',1,60,1),(104,NULL,'TEST-INACTIVE','Disabled category',1,60,0),"
        "(105,104,'TEST-HIDDEN','Hidden category',1,60,1),(106,101,'TEST-INACTIVE-CHILD','Disabled child',1,60,0)");
    Json::Value data(Json::objectValue);for(const auto &a:f.accounts) data[a.first]=a.second.token;
    const auto user=f.accounts.at("demo_reporter01").id;AuthRepository repo(db);
    data["expired"]=newToken();repo.createSession(newOpaqueId(),user,tokenDigest(data["expired"].asString()),"2000-01-01T00:00:00.000Z");
    data["revoked"]=newToken();repo.createSession(newOpaqueId(),user,tokenDigest(data["revoked"].asString()),"2099-01-01T00:00:00.000Z");
    auto q=db.prepare("UPDATE auth_sessions SET revoked_at='2020-01-01T00:00:00.000Z' WHERE token_hash=?1");q.bindText(1,tokenDigest(data["revoked"].asString()));q.step();
    data["fingerprint"]=fingerprint(db);return data;
}
}
int main(int argc,char **argv) {
    std::filesystem::path temp;
    try {
        initializeCrypto();
        if(argc==6&&std::string(argv[1])=="--init") {auto data=init(argv[2],argv[4],argv[5]);std::ofstream out(argv[3]);out<<data;return out?0:1;}
        if(argc==4&&std::string(argv[1])=="--verify") {
            SqliteConnection db(argv[2]);std::ifstream in(argv[3]);Json::Value data;in>>data;check(fingerprint(db)==data["fingerprint"].asString());
            check(db.scalarInt("SELECT count(*) FROM pragma_foreign_key_check")==0);
            auto q=db.prepare("PRAGMA integrity_check");check(q.step()&&q.columnText(0)=="ok");
            std::cout<<"PASS whole-database unchanged; integrity_check=ok; foreign_key_check empty\n";return 0;
        }
        temp=std::filesystem::temp_directory_path()/("campusfix-catalog-"+newOpaqueId());std::filesystem::create_directories(temp);
        auto data=init(temp/"test.sqlite3",CAMPUSFIX_MIGRATIONS_DIR,CAMPUSFIX_SEED_FILE);
        {
            SqliteConnection db(temp/"test.sqlite3");CatalogService service(db);const auto before=fingerprint(db);
            for(const auto *role:{"demo_reporter01","demo_worker01","demo_dispatcher","demo_sysadmin"}) {
                auto roots=service.children(data[role].asString(),true,{});check(roots.size()==2);
                for(const auto &r:roots) check(!r.parentId);
                check(service.children(data[role].asString(),false,{}).size()==4);
            }
            const auto token=data["demo_reporter01"].asString();
            for(const bool locations:{true,false}) {
                check(service.children(token,locations,999999).empty());check(service.children(token,locations,locations?101:104).empty());
                check(service.children(token,locations,locations?102:105).empty());
                const auto child=service.children(token,locations,locations?1:101);check(child.size()==(locations?2:1));
                for(const auto &r:child) check(r.parentId==std::optional<std::int64_t>(locations?1:101));
                try {service.children(token,locations,0);check(false);}catch(const WorkOrderError &e){check(e.status==422);}
            }
            for(const auto *key:{"expired","revoked"}) {try {service.children(data[key].asString(),true,{});check(false);}catch(const WorkOrderError &e){check(e.status==401);}}
            check(service.children(token,false,1).empty());check(fingerprint(db)==before);
        }
        std::filesystem::remove_all(temp);std::cout<<"PASS "<<checks<<" Catalog SQLite/Service checks\n";return 0;
    }catch(const std::exception &e){std::cerr<<e.what()<<'\n';if(!temp.empty()) std::filesystem::remove_all(temp);return 1;}
}
