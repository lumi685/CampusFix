#include "WorkerTestData.h"
#include <json/json.h>
#include <fstream>
#include <iostream>
using namespace campusfix;
int main(int argc,char **argv){try {
    if(argc==6&&std::string(argv[1])=="--init") {
        const auto f=test::workerFixture(argv[2],argv[4],argv[5]);SqliteConnection db(argv[2]);AuthRepository repo(db);Json::Value data;
        for(const auto &a:f.accounts)data[a.first]=a.second.token;
        for(int n=0;n<32;++n){const auto token=newToken();repo.createSession(newOpaqueId(),f.accounts.at("demo_reporter01").id,tokenDigest(token),"2099-01-01T00:00:00.000Z");data["sessions"].append(token);}
        for(const auto *mode:{"expired","revoked","disabled"}) {
            const auto token=newToken();data[mode]=token;
            const auto user=repo.userByName(std::string(mode)=="disabled"?"demo_reporter03":"demo_reporter01");
            repo.createSession(newOpaqueId(),user.id,tokenDigest(token),std::string(mode)=="expired"?"2020-01-01T00:00:00.000Z":"2099-01-01T00:00:00.000Z");
            if(std::string(mode)=="revoked"){auto q=db.prepare("UPDATE auth_sessions SET revoked_at='2020-01-01T00:00:00.000Z' WHERE token_hash=?1");q.bindText(1,tokenDigest(token));q.step();}
        }
        data["fingerprint"]=test::workerFingerprint(db);std::ofstream out(argv[3]);out<<data;return out?0:1;
    }
    if(argc!=3)return 2;
    SqliteConnection db(argv[2]);
    if(std::string(argv[1])=="--snapshot"){std::cout<<test::workerFingerprint(db);return 0;}
    if(std::string(argv[1])=="--verify"){test::integrity(db);std::cout<<"integrity_check=ok; foreign_key_check empty";return 0;}
    return 2;
}catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}}
