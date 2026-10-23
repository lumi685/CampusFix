#pragma once
#include "WorkOrderReadTestData.h"
#include "repositories/StatusLogRepository.h"
#include <sstream>

namespace campusfix::test {
struct TimelineData {ReadData base;std::int64_t total=0;};
inline TimelineData timelineFixture(const std::filesystem::path &path,const std::filesystem::path &migrations,const std::filesystem::path &seed) {
    TimelineData f;f.base=readFixture(path,migrations,seed);SqliteConnection db(path);SqliteTransaction tx(db);
    StatusLogRepository logs(db);
    auto append=[&](const std::string &action,std::optional<std::string> from,const std::string &to,
                    std::optional<std::string> metadata={},bool system=false,std::int64_t order=0) {
        StatusLogEntry e;e.orderId=order?order:f.base.current;
        if(!system) {
            const bool workerEvent=action=="WORKER_ARRIVED"||action=="WORKER_PROGRESS_NOTE";
            const bool reportingEvent=action=="ORDER_CREATED"||action=="ORDER_SUBMITTED"||action=="ORDER_REWORK_REQUESTED";
            e.actorId=f.base.accounts.at(workerEvent?"demo_worker01":reportingEvent?"demo_reporter01":order?"demo_sysadmin":"demo_dispatcher").id;
        }
        e.action=action;e.fromStatus=std::move(from);e.toStatus=to;e.createdAt="2020-02-01T10:00:00.000Z";
        e.requestId="timeline-fixture-"+action;e.publicNote="Fictional public event";e.internalNote="Fixture internal note";e.metadataJson=std::move(metadata);
        if(action=="WORKER_ARRIVED"||action=="WORKER_PROGRESS_NOTE") {
            auto assignment=db.prepare("SELECT id FROM assignments WHERE order_id=?1 ORDER BY id LIMIT 1");assignment.bindInt(1,e.orderId);
            if(assignment.step()) e.assignmentId=assignment.columnInt(0);
        }
        logs.append(e);
    };
    append("ORDER_CREATED",{},"DRAFT");
    append("ORDER_SUBMITTED","DRAFT","PENDING_REVIEW");
    append("ORDER_REVIEWED","PENDING_REVIEW","PENDING_DISPATCH",R"({"review_action":"APPROVE","category_id":1,"approved_urgency":"HIGH","assignment_id":99,"worker_id":99,"candidate_rank":9,"assign_reason":"forbidden on review","transfer_request_id":701,"decision":"APPROVE","rework_count":7,"password_hash":"SECRET_HASH","token_hash":"SECRET_TOKEN_HASH","stored_path":"SECRET_PATH","private":{"secret":"SECRET_METADATA"}})");
    append("ORDER_ASSIGNED","PENDING_DISPATCH","PENDING_ACCEPT",R"({"assignment_id":1,"worker_id":1,"candidate_rank":4,"assign_reason":"Fixture manual choice","category_id":2,"review_action":"APPROVE","transfer_request_id":701,"rework_count":7,"stored_path":"SECRET_PATH","scores":{"private":"SECRET_SCORE"}})");
    append("TRANSFER_APPROVED","PENDING_ACCEPT","PENDING_DISPATCH",R"({"transfer_request_id":701,"decision":"APPROVE","worker_id":1,"assign_reason":"forbidden on transfer","rework_count":7})");
    append("ORDER_REWORK_REQUESTED","PENDING_VERIFY","IN_PROGRESS",R"({"rework_count":1,"review_action":"APPROVE","worker_id":1,"decision":"APPROVE"})");
    append("UNKNOWN_AUDIT","IN_PROGRESS","IN_PROGRESS",R"({"assignment_id":1,"category_id":1,"rework_count":1})",true);
    append("ORDER_REVIEWED","PENDING_REVIEW","PENDING_DISPATCH","{broken-json");
    append("ORDER_ASSIGNED","PENDING_DISPATCH","PENDING_ACCEPT",R"({"assignment_id":"1","worker_id":[1],"candidate_rank":true,"assign_reason":{"stored_path":"SECRET_PATH"}})");
    append("ORDER_RETURNED","PENDING_REVIEW","PENDING_DISPATCH",R"({"review_action":"APPROVE","category_id":1})"); // Deliberately inconsistent historical fixture: no management projection.
    append("WORKER_ARRIVED","IN_PROGRESS","IN_PROGRESS");
    append("WORKER_PROGRESS_NOTE","IN_PROGRESS","IN_PROGRESS");
    std::string deep(80,'[');deep+="0";deep+=std::string(80,']');
    append("ORDER_REVIEWED","PENDING_REVIEW","PENDING_DISPATCH",deep);
    append("ORDER_REVIEWED","PENDING_REVIEW","PENDING_DISPATCH",R"({"review_action":"APPROVE","review_action":"RETURN","category_id":1})");
    append("ORDER_REVIEWED","PENDING_REVIEW","PENDING_DISPATCH",R"({"review_action":"APPROVE","category_id":1})",false,f.base.foreign);
    append("WORKER_PROGRESS_NOTE","IN_PROGRESS","IN_PROGRESS",R"({"private":"SECRET_METADATA"})",false,f.base.history);
    auto q=db.prepare("SELECT count(*) FROM status_logs WHERE order_id=?1");q.bindInt(1,f.base.current);q.step();f.total=q.columnInt(0);
    tx.commit();return f;
}
// Include every persisted log field to detect UPDATE/DELETE as well as extra INSERTs in read tests.
inline std::string timelineFingerprint(SqliteConnection &db) {
    auto q=db.prepare("SELECT id,order_id,actor_id,assignment_id,action,from_status,to_status,public_note,internal_note,metadata_json,created_at,request_id FROM status_logs ORDER BY id");
    std::ostringstream output;
    while(q.step()) for(int i=0;i<12;++i) {
        if(q.columnIsNull(i)) output<<"N;";
        else {const auto v=q.columnText(i);output<<v.size()<<':'<<v<<';';}
    }
    return readFingerprint(db)+"|events="+output.str();
}
inline void timelineGrantMode(SqliteConnection &db,const std::string &mode) {
    if(mode=="admin-mismatch") {readGrantMode(db,"admin-building");return;}
    if(mode=="revoke-business") {readGrantMode(db,"revoke-admin");return;}
    if(mode=="revoke-dispatcher") {readGrantMode(db,mode);return;}
    SqliteTransaction tx(db);
    if(mode=="revoke-internal") db.execute("UPDATE management_grants SET revoked_at='2026-01-01T00:00:00.000Z' WHERE capability='INTERNAL_NOTE_READ' AND user_id=(SELECT id FROM users WHERE username='demo_sysadmin')");
    else if(mode=="admin-match") {
        db.execute("UPDATE management_grants SET revoked_at='2026-01-01T00:00:00.000Z' WHERE user_id=(SELECT id FROM users WHERE username='demo_sysadmin') AND revoked_at IS NULL");
        db.execute("INSERT INTO management_grants(user_id,capability,scope_location_id,granted_by) SELECT id,'BUSINESS_READ',3,id FROM users WHERE username='demo_sysadmin'");
        db.execute("INSERT INTO management_grants(user_id,capability,scope_location_id,granted_by) SELECT id,'INTERNAL_NOTE_READ',11,id FROM users WHERE username='demo_sysadmin'");
    } else throw std::runtime_error("Unknown timeline fixture grant mode");
    tx.commit();
}
}
