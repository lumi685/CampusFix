#include "TimelineTestData.h"
#include <json/json.h>
#include <fstream>
#include <iostream>

using namespace campusfix;
int main(int argc,char **argv) {
    try {
        if(argc==6&&std::string(argv[1])=="--init") {
            const auto f=test::timelineFixture(argv[2],argv[4],argv[5]);Json::Value data(Json::objectValue);
            for(const auto &a:f.base.accounts) data[a.first]=a.second.token;
            data["current"]=Json::Int64(f.base.current);data["history"]=Json::Int64(f.base.history);
            data["foreign"]=Json::Int64(f.base.foreign);data["total"]=Json::Int64(f.total);
            data["empty"]=Json::Int64(f.base.draft);
            SqliteConnection db(argv[2]);data["fingerprint"]=test::timelineFingerprint(db);
            data["expected_public"]=Json::Value(Json::arrayValue);
            // Independent SQL projection to compare actual HTTP fields with persisted SQLite data.
            auto q=db.prepare("SELECT s.id,s.action,s.from_status,s.to_status,s.public_note,s.actor_id,u.display_name,s.created_at "
                "FROM status_logs s LEFT JOIN users u ON u.id=s.actor_id WHERE s.order_id=?1 ORDER BY s.created_at,s.id");q.bindInt(1,f.base.current);
            while(q.step()) {
                Json::Value row(Json::objectValue);row["id"]=Json::Int64(q.columnInt(0));row["action"]=q.columnText(1);
                row["from_status"]=q.columnIsNull(2)?Json::Value(Json::nullValue):Json::Value(q.columnText(2));
                row["to_status"]=q.columnText(3);row["public_note"]=q.columnIsNull(4)?Json::Value(Json::nullValue):Json::Value(q.columnText(4));
                row["actor"]=Json::nullValue;
                if(!q.columnIsNull(5)) {row["actor"]["id"]=Json::Int64(q.columnInt(5));row["actor"]["display_name"]=q.columnText(6);}
                row["created_at"]=q.columnText(7);data["expected_public"].append(row);
            }
            std::ofstream output(argv[3],std::ios::binary);output<<data;
            if(!output) throw std::runtime_error("Cannot write timeline fixture");return 0;
        }
        if(argc==4&&std::string(argv[1])=="--grant") {SqliteConnection db(argv[2]);test::timelineGrantMode(db,argv[3]);return 0;}
        if(argc==4&&std::string(argv[1])=="--verify") {
            SqliteConnection db(argv[2]);std::ifstream input(argv[3]);Json::Value before;input>>before;
            if(test::timelineFingerprint(db)!=before["fingerprint"].asString()) throw std::runtime_error("HTTP changed order state or any persisted log field");
            if(db.scalarInt("SELECT count(*) FROM pragma_foreign_key_check")) throw std::runtime_error("Foreign key violation");
            auto q=db.prepare("PRAGMA integrity_check");if(!q.step()||q.columnText(0)!="ok") throw std::runtime_error("Integrity failure");
            std::cout<<"PASS timeline HTTP unchanged full log/order fingerprint; integrity_check=ok; foreign_key_check empty\n";return 0;
        }
        return 2;
    }catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}
}
