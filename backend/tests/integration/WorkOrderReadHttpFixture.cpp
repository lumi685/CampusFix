#include "WorkOrderReadTestData.h"
#include <json/json.h>
#include <fstream>
#include <iostream>

using namespace campusfix;
int main(int argc,char **argv) {
    try {
        if(argc==6&&std::string(argv[1])=="--init") {
            const auto fixture=test::readFixture(argv[2],argv[4],argv[5]);Json::Value data(Json::objectValue);
            for(const auto &a:fixture.accounts) data[a.first]=a.second.token;
            data["current"]=Json::Int64(fixture.current);data["history"]=Json::Int64(fixture.history);
            data["foreign"]=Json::Int64(fixture.foreign);data["draft"]=Json::Int64(fixture.draft);
            SqliteConnection db(argv[2]);data["fingerprint"]=test::readFingerprint(db);
            std::ofstream output(argv[3],std::ios::binary);output<<data;
            if(!output) throw std::runtime_error("Cannot write read fixture");return 0;
        }
        if(argc==4&&std::string(argv[1])=="--grant") {SqliteConnection db(argv[2]);test::readGrantMode(db,argv[3]);return 0;}
        if(argc==4&&std::string(argv[1])=="--verify") {
            SqliteConnection db(argv[2]);std::ifstream input(argv[3]);Json::Value before;input>>before;
            if(test::readFingerprint(db)!=before["fingerprint"].asString()) throw std::runtime_error("Read HTTP changed orders or logs");
            if(db.scalarInt("SELECT count(*) FROM pragma_foreign_key_check")) throw std::runtime_error("Foreign key violation");
            auto q=db.prepare("PRAGMA integrity_check");if(!q.step()||q.columnText(0)!="ok") throw std::runtime_error("Integrity failure");
            std::cout<<"PASS post-HTTP order state/version/timestamps/log fingerprint unchanged; integrity_check=ok; foreign_key_check empty\n";
            return 0;
        }
        return 2;
    }catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}
}
