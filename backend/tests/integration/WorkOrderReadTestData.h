#pragma once
// Test-only SQLite fixtures: no business assignment/review endpoint is implemented here.
#include "infrastructure/DatabaseMigration.h"
#include "infrastructure/DemoSeed.h"
#include "infrastructure/AuthCrypto.h"
#include "repositories/AuthRepository.h"
#include "repositories/WorkOrderRepository.h"
#include <map>

namespace campusfix::test {
struct ReadAccount {std::int64_t id;std::string token;};
struct ReadData {std::map<std::string,ReadAccount> accounts;std::int64_t current=0,history=0,foreign=0,draft=0;};
inline ReadData readFixture(const std::filesystem::path &path,const std::filesystem::path &migrations,const std::filesystem::path &seed) {
    initializeCrypto();applyMigrations(path,migrations);applyDemoSeed(path,seed);
    SqliteConnection db(path);ReadData f;
    db.execute("INSERT INTO users(username,password_hash,display_name,role,status) VALUES"
        "('test_no_grants','!UNSET_DEMO_CREDENTIAL!','Test no grants','sysadmin','ACTIVE')");
    for(const auto *name:{"demo_reporter01","demo_reporter02","demo_worker01","demo_worker02","demo_dispatcher","demo_sysadmin","test_no_grants"}) {
        auto active=db.prepare("UPDATE users SET status='ACTIVE' WHERE username=?1");active.bindText(1,name);active.step();
        AuthRepository repo(db);const auto user=repo.userByName(name);const auto token=newToken();
        repo.createSession(newOpaqueId(),user.id,tokenDigest(token),"2099-01-01T00:00:00.000Z");
        f.accounts[name]={user.id,token};
    }
    WorkOrderRepository repo(db);
    WorkOrderInput input{1,11,"Read current task","DO_NOT_LEAK_DESCRIPTION","NORMAL","Fictional contact","00000000000",{}};
    f.current=repo.create(f.accounts.at("demo_reporter01").id,input,"2020-01-01T00:00:00.000Z").id;
    input.title="Read historical task";
    f.history=repo.create(f.accounts.at("demo_reporter02").id,input,"2020-01-01T00:00:00.000Z").id;
    input.title="Read other worker task";input.locationId=13;
    f.foreign=repo.create(f.accounts.at("demo_reporter02").id,input,"2020-01-01T00:00:00.000Z").id;
    input.title="Read draft";input.locationId=11;input.categoryId=2;
    f.draft=repo.create(f.accounts.at("demo_reporter01").id,input,"2020-01-01T00:00:00.000Z").id;
    db.execute("UPDATE work_orders SET status='PENDING_ACCEPT',order_no=printf('WX20200101%04d',id),submitted_at='2020-01-01T08:00:00.000Z' WHERE title IN ('Read current task','Read historical task','Read other worker task')");
    db.execute("UPDATE work_orders SET created_at='2020-01-01T00:00:00.000Z'");
    auto assign=[&](std::int64_t order,int worker,bool ended) {
        auto q=db.prepare("INSERT INTO assignments(order_id,worker_id,assigned_by,assign_reason,ended_at,end_reason,assigned_at) VALUES(?1,?2,?3,?4,?5,?6,'2020-01-01T09:00:00.000Z')");
        q.bindInt(1,order);q.bindInt(2,worker);q.bindInt(3,f.accounts.at("demo_dispatcher").id);
        q.bindText(4,"DO_NOT_LEAK_ASSIGN_REASON");
        if(ended) {q.bindText(5,"2020-01-02T00:00:00.000Z");q.bindText(6,"TRANSFERRED");}
        else {q.bindNull(5);q.bindNull(6);}
        q.step();
    };
    assign(f.current,1,false);assign(f.history,1,true);assign(f.history,1,true);assign(f.history,2,false);assign(f.foreign,2,false);
    repo.log(f.current,f.accounts.at("demo_dispatcher").id,"ORDER_REVIEWED","PENDING_REVIEW","PENDING_DISPATCH","2020-01-01T00:00:00.000Z","read-fixture");
    db.execute("UPDATE status_logs SET internal_note='DO_NOT_LEAK_INTERNAL',metadata_json='{\"private\":\"DO_NOT_LEAK_METADATA\"}' WHERE request_id='read-fixture'");
    auto attachment=db.prepare("INSERT INTO attachments(order_id,uploader_id,purpose,original_name,stored_path,mime,size_bytes,sha256) VALUES(?1,?2,'REPORT','fictional.png','DO_NOT_LEAK_STORED_PATH','image/png',1,'test-only')");
    attachment.bindInt(1,f.current);attachment.bindInt(2,f.accounts.at("demo_reporter01").id);attachment.step();
    return f;
}
inline std::string readFingerprint(SqliteConnection &db) {
    auto q=db.prepare("SELECT group_concat(row) FROM (SELECT id||':'||status||':'||version||':'||coalesce(order_no,'')||':'||updated_at AS row FROM work_orders ORDER BY id)");
    q.step();const auto orders=q.columnText(0);
    return orders+"|logs="+std::to_string(db.scalarInt("SELECT count(*) FROM status_logs"));
}
inline void readGrantMode(SqliteConnection &db,const std::string &mode) {
    SqliteTransaction tx(db);
    if(mode=="revoke-dispatcher") db.execute("UPDATE management_grants SET revoked_at='2026-01-01T00:00:00.000Z' WHERE user_id=(SELECT id FROM users WHERE username='demo_dispatcher')");
    else if(mode=="disable-reporter") db.execute("UPDATE users SET status='DISABLED' WHERE username='demo_reporter01'");
    else {
        db.execute("UPDATE management_grants SET revoked_at='2026-01-01T00:00:00.000Z' WHERE user_id=(SELECT id FROM users WHERE username='demo_sysadmin') AND revoked_at IS NULL AND capability IN ('BUSINESS_READ','AREA_SCOPE')");
        if(mode!="revoke-admin") {
            if(mode!="admin-global"&&mode!="admin-building"&&mode!="admin-area-only"&&mode!="admin-room"&&mode!="admin-read-only") throw std::runtime_error("Unknown test mode");
            if(mode=="admin-read-only") db.execute("UPDATE management_grants SET revoked_at='2026-01-01T00:00:00.000Z' WHERE user_id=(SELECT id FROM users WHERE username='demo_sysadmin') AND capability='INTERNAL_NOTE_READ' AND revoked_at IS NULL");
            auto q=db.prepare("INSERT INTO management_grants(user_id,capability,scope_location_id,granted_by) VALUES("
                "(SELECT id FROM users WHERE username='demo_sysadmin'),?1,?2,(SELECT id FROM users WHERE username='demo_sysadmin'))");
            q.bindText(1,mode=="admin-area-only"?"AREA_SCOPE":"BUSINESS_READ");
            if(mode=="admin-global") q.bindNull(2);else q.bindInt(2,mode=="admin-room"?11:mode=="admin-read-only"?2:3);
            q.step();
        }
    }
    tx.commit();
}
}
