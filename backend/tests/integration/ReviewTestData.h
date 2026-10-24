#pragma once
#include "WorkOrderReadTestData.h"
#include "services/WorkOrderService.h"
#include <sstream>

namespace campusfix::test {
inline ReviewInput approve() {return {"APPROVE",2,2,std::string("HIGH"),{},std::string("REVIEW_PRIVATE_MARKER")};}
inline ReviewInput outcome(const std::string &action) {return {action,2,{},{},std::string("Fictional public reason"),std::string("REVIEW_PRIVATE_MARKER")};}
inline WorkOrder pending(SqliteConnection &db,const ReadData &f,const std::string &owner="demo_reporter01",std::int64_t location=11) {
    const auto &a=f.accounts.at(owner);WorkOrderService service(db);
    WorkOrderInput input{1,location,"Fictional review test","Fictional review description","NORMAL","Fictional contact","00000000000",{}};
    const auto draft=service.create(a.id,a.token,input,"review-fixture-create");
    return service.submit(a.id,a.token,draft.id,draft.version,"review-fixture-submit");
}
inline void grant(SqliteConnection &db,const std::string &name,const std::string &cap,std::optional<std::int64_t> scope) {
    auto q=db.prepare("INSERT INTO management_grants(user_id,capability,scope_location_id,granted_by) SELECT id,?1,?2,id FROM users WHERE username=?3");
    q.bindText(1,cap);if(scope) q.bindInt(2,*scope);else q.bindNull(2);q.bindText(3,name);q.step();
}
inline void revoke(SqliteConnection &db,const std::string &name,const std::string &cap) {
    auto q=db.prepare("UPDATE management_grants SET revoked_at='2020-01-01T00:00:00.000Z' WHERE user_id=(SELECT id FROM users WHERE username=?1) AND capability=?2 AND revoked_at IS NULL");
    q.bindText(1,name);q.bindText(2,cap);q.step();
}
// Every column of every mutable business row, not merely counts or state/version.
inline std::string reviewFingerprint(SqliteConnection &db) {
    std::ostringstream out;
    for(const auto *table:{"work_orders","status_logs","assignments","dispatch_records","order_number_sequences"}) {
        out<<table;
        auto q=db.prepare(std::string("SELECT * FROM ")+table+" ORDER BY 1");
        auto cols=db.prepare(std::string("SELECT count(*) FROM pragma_table_info('")+table+"')");cols.step();const auto n=cols.columnInt(0);
        while(q.step()) for(int i=0;i<n;++i) {
            if(q.columnIsNull(i)) out<<"N;";
            else {const auto v=q.columnText(i);out<<v.size()<<':'<<v<<';';}
        }
    }
    return tokenDigest(out.str());
}
inline void integrity(SqliteConnection &db) {
    if(db.scalarInt("SELECT count(*) FROM pragma_foreign_key_check")) throw std::runtime_error("foreign_key_check not empty");
    auto q=db.prepare("PRAGMA integrity_check");if(!q.step()||q.columnText(0)!="ok") throw std::runtime_error("integrity_check failed");
}
}
