// Test-only provisioning for real frontend HTTP/browser verification. Random
// passwords live only in the caller's disposable credential file, never stdout.
#include "DispatchTestData.h"
#include "services/DemoAccountActivation.h"
#include <fstream>
#include <iostream>
using namespace campusfix;
int main(int argc,char **argv) {
    try {
        initializeCrypto();
        if(argc==6&&std::string(argv[1])=="--init") {
            const std::filesystem::path path=argv[2];applyMigrations(path,argv[4]);applyDemoSeed(path,argv[5]);Json::Value data;
            for(const auto *name:{"demo_dispatcher","demo_sysadmin","demo_reporter01"}) {
                const auto password=newToken();setDemoPassword(path,name,password);enableDemoAccount(path,name);data["passwords"][name]=password;
            }
            const auto f=test::dispatchFixture(path,argv[4],argv[5]);SqliteConnection db(path);
            for(const auto &a:f.accounts)data[a.first]=a.second.token;
            // Prove the independent DISPATCH-only sysadmin case, not a guessed
            // conjunction with the unrelated seed BUSINESS_READ capability.
            test::revoke(db,"demo_sysadmin","BUSINESS_READ");test::revoke(db,"demo_sysadmin","INTERNAL_NOTE_READ");
            test::grant(db,"demo_sysadmin","DISPATCH",3);
            for(const auto *key:{"top","fourth","conflict","ineligible","unknown","empty","browser_top","browser_fourth","browser_empty"})data[key]=Json::Int64(test::dispatchOrder(db,f).id);
            data["foreign"]=Json::Int64(test::dispatchOrder(db,f,13).id);
            std::ofstream file(argv[3],std::ios::binary);file<<data;if(!file)throw std::runtime_error("Fixture output failed");return 0;
        }
        if(argc==4&&std::string(argv[1])=="--mode") {
            SqliteConnection db(argv[2]);SqliteTransaction tx(db);const std::string mode=argv[3];
            if(mode=="no-coverage")db.execute("UPDATE workers SET service_area_id=2");
            else if(mode=="coverage")db.execute("UPDATE workers SET service_area_id=1");
            else throw std::runtime_error("Unknown fixture mode");
            tx.commit();return 0;
        }
        return 2;
    }catch(const std::exception &){std::cerr<<"Isolated dispatch UI fixture failed\n";return 1;}
}
