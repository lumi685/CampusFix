#pragma once
#include "ReviewTestData.h"
#include "services/WorkerService.h"

namespace campusfix::test {
inline WorkerInput workerInput(std::int64_t area=1) {return {area,3,"AVAILABLE",true,true,{{1,5,true},{2,3,false}}};}
inline std::string workerFingerprint(SqliteConnection &db) {
    sqlite3_int64 size=0;auto *bytes=sqlite3_serialize(db.nativeHandle(),"main",&size,0);
    if(!bytes)throw std::runtime_error("Cannot fingerprint test database");
    const std::string raw(reinterpret_cast<char *>(bytes),static_cast<std::size_t>(size));sqlite3_free(bytes);return tokenDigest(raw);
}
inline ReadData workerFixture(const std::filesystem::path &path,const std::filesystem::path &migrations,const std::filesystem::path &seed) {
    auto data=readFixture(path,migrations,seed);SqliteConnection db(path);
    db.execute("INSERT INTO users(username,password_hash,display_name,role,status) VALUES('test_dispatcher_b','!UNSET_DEMO_CREDENTIAL!','Fictional Dispatcher B','dispatcher','ACTIVE')");
    AuthRepository auth(db);const auto user=auth.userByName("test_dispatcher_b");const auto token=newToken();auth.createSession(newOpaqueId(),user.id,tokenDigest(token),"2099-01-01T00:00:00.000Z");data.accounts["test_dispatcher_b"]={user.id,token};
    grant(db,"test_dispatcher_b","AREA_SCOPE",2);
    WorkOrderInput input{1,11,"Fictional second active assignment","Fictional capacity test","NORMAL","Fictional contact","00000000000",{}};
    const auto order=WorkOrderRepository(db).create(data.accounts.at("demo_reporter01").id,input,"2020-01-01T00:00:00.000Z");
    auto update=db.prepare("UPDATE work_orders SET status='PENDING_ACCEPT' WHERE id=?1");update.bindInt(1,order.id);update.step();
    auto assignment=db.prepare("INSERT INTO assignments(order_id,worker_id,assigned_by) VALUES(?1,1,?2)");assignment.bindInt(1,order.id);assignment.bindInt(2,data.accounts.at("demo_dispatcher").id);assignment.step();
    return data;
}
inline std::string scalarText(SqliteConnection &db,const std::string &sql) {auto q=db.prepare(sql);if(!q.step())throw std::runtime_error("Missing test row");return q.columnText(0);}
}
