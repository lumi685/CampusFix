#pragma once
// Fictional isolated SQLite fixtures only. Never used by production startup.
#include "ReviewTestData.h"
#include "services/AssignmentService.h"
#include <array>
#include <json/json.h>

namespace campusfix::test {
inline ReadData dispatchFixture(const std::filesystem::path &path,const std::filesystem::path &migrations,const std::filesystem::path &seed) {
    initializeCrypto();applyMigrations(path,migrations);applyDemoSeed(path,seed);SqliteConnection db(path);ReadData f;
    for(const auto *name:{"demo_reporter01","demo_reporter02","demo_worker01","demo_dispatcher","demo_sysadmin"}) {
        auto q=db.prepare("UPDATE users SET status='ACTIVE' WHERE username=?1");q.bindText(1,name);q.step();
        AuthRepository repo(db);const auto user=repo.userByName(name);const auto token=newToken();
        repo.createSession(newOpaqueId(),user.id,tokenDigest(token),"2099-01-01T00:00:00.000Z");f.accounts[name]={user.id,token};
    }
    // Six genuinely eligible candidates, not a fake recommendation: the production
    // algorithm must compute their order from these real database facts.
    db.execute("UPDATE users SET status='ACTIVE' WHERE role='worker'");
    db.execute("UPDATE workers SET service_area_id=1,availability='AVAILABLE',active=1,max_concurrent_tasks=3,emergency_duty=0");
    db.execute("INSERT INTO worker_skills(worker_id,skill_id,level,certified,active) SELECT id,1,max(1,6-id),1,1 FROM workers WHERE 1 ON CONFLICT(worker_id,skill_id) DO UPDATE SET level=excluded.level,certified=1,active=1");
    return f;
}
inline WorkOrder dispatchOrder(SqliteConnection &db,const ReadData &f,std::int64_t location=11) {
    const auto pendingOrder=pending(db,f,"demo_reporter01",location);auto input=approve();input.categoryId=1;input.approvedUrgency="HIGH";
    const auto &a=f.accounts.at("demo_dispatcher");
    if(location==13) { // This branch is fixture setup, not an authorization bypass in an API.
        auto q=db.prepare("UPDATE work_orders SET status='PENDING_DISPATCH',approved_urgency='HIGH',version=3 WHERE id=?1");q.bindInt(1,pendingOrder.id);q.step();
        return WorkOrderRepository(db).find(pendingOrder.id);
    }
    WorkOrderService(db).review(a.id,a.token,pendingOrder.id,input,"dispatch-fixture-review");
    return WorkOrderRepository(db).find(pendingOrder.id);
}
inline const std::array<const char *,16> dispatchTables={"users","auth_sessions","locations","management_grants","skills","fault_categories","workers","worker_skills","order_number_sequences","work_orders","attachments","assignments","transfer_requests","status_logs","dispatch_records","evaluations"};
inline std::string dispatchFingerprint(SqliteConnection &db) {
    std::ostringstream out;
    for(const auto *table:dispatchTables) {
        out<<table<<'|';auto cols=db.prepare(std::string("SELECT count(*) FROM pragma_table_info('")+table+"')");cols.step();const auto n=cols.columnInt(0);
        auto q=db.prepare(std::string("SELECT * FROM ")+table+" ORDER BY "+(std::string(table)=="worker_skills"?"1,2":"1"));
        while(q.step()) {out<<'[';for(int i=0;i<n;++i) {if(q.columnIsNull(i))out<<"N;";else {const auto s=q.columnText(i);out<<s.size()<<':'<<s<<';';}}out<<']';}
    }
    return tokenDigest(out.str());
}
inline Json::Value inspectDispatch(SqliteConnection &db,std::int64_t id) {
    Json::Value value(Json::objectValue);auto q=db.prepare("SELECT status,version FROM work_orders WHERE id=?1");q.bindInt(1,id);
    if(!q.step())throw std::runtime_error("fixture order missing");value["status"]=q.columnText(0);value["version"]=Json::Int64(q.columnInt(1));
    for(const auto *table:{"assignments","dispatch_records","status_logs"}) {
        auto rows=db.prepare(std::string("SELECT * FROM ")+table+" WHERE order_id=?1 ORDER BY id");rows.bindInt(1,id);
        auto columns=db.prepare(std::string("SELECT name FROM pragma_table_info('")+table+"')");std::vector<std::string> names;while(columns.step())names.push_back(columns.columnText(0));
        value[table]=Json::arrayValue;while(rows.step()) {Json::Value row(Json::objectValue);for(std::size_t i=0;i<names.size();++i)row[names[i]]=rows.columnIsNull(static_cast<int>(i))?Json::Value():Json::Value(rows.columnText(static_cast<int>(i)));value[table].append(row);}
    }
    value["worker_loads"]=Json::arrayValue;auto loads=db.prepare("SELECT w.id,count(a.id) FROM workers w LEFT JOIN assignments a ON a.worker_id=w.id AND a.ended_at IS NULL GROUP BY w.id ORDER BY w.id");
    while(loads.step()){Json::Value row;row["worker_id"]=Json::Int64(loads.columnInt(0));row["active_tasks"]=Json::Int64(loads.columnInt(1));value["worker_loads"].append(row);}return value;
}
}
