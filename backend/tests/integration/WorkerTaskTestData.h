#pragma once
#include "DispatchTestData.h"
#include "services/WorkerTaskService.h"
namespace campusfix::test {
inline ReadData taskFixture(const std::filesystem::path &path,const std::filesystem::path &migrations,const std::filesystem::path &seed){
    auto f=dispatchFixture(path,migrations,seed);SqliteConnection db(path);AuthRepository auth(db);const auto user=auth.userByName("demo_worker02");const auto token=newToken();auth.createSession(newOpaqueId(),user.id,tokenDigest(token),"2099-01-01T00:00:00.000Z");f.accounts["demo_worker02"]={user.id,token};db.execute("UPDATE workers SET max_concurrent_tasks=100");return f;
}
inline AssignmentResult taskOrder(SqliteConnection &db,const ReadData &f,std::int64_t worker=1){const auto o=dispatchOrder(db,f);const auto &admin=f.accounts.at("demo_dispatcher");return AssignmentService(db).assign(admin.id,admin.token,o.id,{worker,3,std::string("Fictional test assignment")},"worker-task-fixture-assignment");}
inline WorkerTaskResult taskAction(SqliteConnection &db,const ReadAccount &a,std::int64_t id,std::int64_t version=4,std::optional<std::string> reason={}){return WorkerTaskService(db).perform(a.id,a.token,id,version,reason,"worker-task-test-request");}
inline void pendingTransfer(SqliteConnection &db,const AssignmentResult &o,std::int64_t user){auto q=db.prepare("INSERT INTO transfer_requests(order_id,assignment_id,requested_by,reason,order_version) VALUES(?1,?2,?3,'Fictional pending transfer',4)");q.bindInt(1,o.orderId);q.bindInt(2,o.assignmentId);q.bindInt(3,user);q.step();}
inline std::int64_t activeTaskCount(SqliteConnection &db,std::int64_t worker=1){auto q=db.prepare("SELECT count(*) FROM assignments WHERE worker_id=?1 AND ended_at IS NULL");q.bindInt(1,worker);q.step();return q.columnInt(0);}
}
