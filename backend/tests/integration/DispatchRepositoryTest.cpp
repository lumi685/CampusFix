#include "repositories/DispatchRepository.h"
#include "dispatch/WeightedScoreStrategy.h"
#include "infrastructure/DatabaseMigration.h"
#include "infrastructure/DemoSeed.h"
#include "models/WorkOrder.h"

#include <chrono>
#include <cmath>
#include <functional>
#include <iostream>
#include <limits>
#include <sstream>

using namespace campusfix;
namespace {
int checks = 0, cases = 0;
void check(bool condition, const std::string &message) {
    if (!condition) throw std::runtime_error(message);
    ++checks;
}
template<class F> void rejects(F call, const std::string &message) {
    bool rejected = false;
    try { call(); } catch (const std::exception &) { rejected = true; }
    check(rejected, message);
}
const dispatch::DispatchWorker &worker(const DispatchData &data, std::int64_t id) {
    for (const auto &item : data.workers) if (item.record.id == id) return item;
    throw std::runtime_error("Expected fixture worker is absent");
}
void order(SqliteConnection &db, std::int64_t id, const std::string &status = "COMPLETED") {
    auto q = db.prepare("INSERT INTO work_orders(id,reporter_id,category_id,location_id,title,description,"
        "contact_name,contact_phone,status,approved_urgency) VALUES(?1,3,1,11,'Fictional','Fictional',"
        "'Fictional','00000000000',?2,'NORMAL')");
    q.bindInt(1, id); q.bindText(2, status); q.step();
}
void assignment(SqliteConnection &db, std::int64_t id, std::int64_t orderId, std::int64_t workerId,
                std::optional<std::string> endReason = std::string("COMPLETED"),
                std::optional<std::string> accepted = std::string("2020-01-01T00:00:00Z"),
                std::optional<std::string> submitted = std::string("2020-01-01T04:00:00Z"),
                std::optional<std::string> ended = std::string("2020-01-01T05:00:00Z")) {
    auto q = db.prepare("INSERT INTO assignments(id,order_id,worker_id,assigned_by,assigned_at,accepted_at,"
        "result_submitted_at,ended_at,end_reason) VALUES(?1,?2,?3,2,'2020-01-01T00:00:00Z',?4,?5,?6,?7)");
    q.bindInt(1, id); q.bindInt(2, orderId); q.bindInt(3, workerId);
    const auto bind = [&](int n, const std::optional<std::string> &s) { if (s) q.bindText(n, *s); else q.bindNull(n); };
    bind(4, accepted); bind(5, submitted); bind(6, ended); bind(7, endReason); q.step();
}
void rating(SqliteConnection &db, std::int64_t id, std::int64_t orderId, int stars = 4,
            const std::string &created = "2020-01-02T00:00:00Z") {
    auto q = db.prepare("INSERT INTO evaluations(id,order_id,reporter_id,score,created_at) VALUES(?1,?2,3,?3,?4)");
    q.bindInt(1, id); q.bindInt(2, orderId); q.bindInt(3, stars); q.bindText(4, created); q.step();
}
std::string fingerprint(SqliteConnection &db) {
    std::ostringstream out;
    auto tables = db.prepare("SELECT name FROM sqlite_schema WHERE type='table' AND name NOT LIKE 'sqlite_%' ORDER BY name");
    while (tables.step()) {
        const auto name = tables.columnText(0);
        auto q = db.prepare("SELECT * FROM \"" + name + "\" ORDER BY 1,2");
        auto info = db.prepare("SELECT count(*) FROM pragma_table_info(?1)"); info.bindText(1, name); info.step();
        out << name;
        while (q.step()) for (int i = 0; i < info.columnInt(0); ++i) {
            if (q.columnIsNull(i)) out << "NULL;";
            else {const auto text = q.columnText(i); out << text.size() << ':' << text << ';';}
        }
    }
    return out.str();
}
struct QueryTrace { int selects = 0; bool readonly = true; };
int trace(unsigned, void *context, void *statement, void *) {
    auto &state = *static_cast<QueryTrace *>(context);
    auto *sql = static_cast<sqlite3_stmt *>(statement);
    if (std::string(sqlite3_sql(sql)).find("SELECT") == 0) ++state.selects;
    state.readonly = state.readonly && sqlite3_stmt_readonly(sql) != 0;
    return 0;
}
int deny(void *context, int action, const char *name, const char *, const char *, const char *) {
    return action == SQLITE_READ && name && *static_cast<std::string *>(context) == name ? SQLITE_DENY : SQLITE_OK;
}
}

int main() {
    const auto root = std::filesystem::temp_directory_path() /
        ("campusfix-dispatch-repository-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directories(root);
    try {
        const auto path = root / "dispatch.sqlite3";
        applyMigrations(path, CAMPUSFIX_MIGRATIONS_DIR);
        applyDemoSeed(path, CAMPUSFIX_SEED_FILE);
        {
            SqliteConnection db(path);
            db.execute("UPDATE work_orders SET status='PENDING_DISPATCH',approved_urgency='HIGH',requested_urgency='URGENT',version=4 WHERE id=1");
            const auto load = [&] { return DispatchRepository(db).load(1); };
            const auto test = [&](const std::string &name, const std::function<void()> &call) {
                SqliteTransaction transaction(db); // Fixture changes always roll back.
                call(); ++cases; std::cout << "PASS DispatchRepository " << name << '\n';
            };
            test("reviewed order and strict candidate pool", [&] {
                const auto result = load();
                check(result.order.id == 1 && result.order.version == 4 && result.order.status == "PENDING_DISPATCH", "actual order/version/state");
                check(result.order.approvedUrgency == "HIGH", "never substitute requested urgency");
                check(result.order.categoryId == 1 && result.order.requiredSkill.id == 1 && result.order.requiredSkill.requiresCertificate && result.order.categoryRequiresCertificate, "real reviewed category and both qualification inputs");
                check(result.workers.size() == 3 && result.workers[0].record.id == 1 && result.workers[2].record.id == 3, "pool covers all matching unpaged profiles but not unrelated campus");
                for (const auto &w : result.workers) {
                    check(w.accountStatus == "DISABLED", "do not hide disabled accounts");
                    check(w.skillsLoaded && w.history.efficiency && w.history.rating, "successful query marks complete raw reads");
                    check(w.history.efficiency->empty() && w.history.rating->empty(), "zero actual history stays empty, not fake 60");
                }
                check(worker(result, 3).record.availability == "OFF_DUTY", "do not prefilter availability");
            });
            test("reviewed category actually changes input", [&] {
                db.execute("UPDATE work_orders SET category_id=3 WHERE id=1"); const auto result = load();
                check(result.order.categoryId == 3 && result.order.requiredSkill.id == 3 && !result.order.requiredSkill.requiresCertificate && !result.order.categoryRequiresCertificate, "category join follows reviewed database row");
            });
            test("truthful account/profile/skill hydration", [&] {
                db.execute("UPDATE users SET status='ACTIVE',role='reporter' WHERE id=6; UPDATE workers SET active=0,emergency_duty=1,availability='BUSY',max_concurrent_tasks=7 WHERE id=1");
                const auto data = load(); const auto &w = worker(data, 1);
                check(w.accountRole == "reporter" && w.accountStatus == "ACTIVE" && !w.record.active && w.record.emergencyDuty && w.record.availability == "BUSY" && w.record.capacity == 7, "all persisted state, including invalid business role, reaches filter");
                check(w.record.skills.size() == 1 && w.record.skills[0].skillId == 1 && w.record.skills[0].level == 5 && w.record.skills[0].certified, "real required-skill proficiency and certificate");
            });
            test("inactive skill relationships are not credentials", [&] {
                db.execute("UPDATE worker_skills SET active=0 WHERE worker_id=1; UPDATE skills SET active=0 WHERE id=2");
                const auto data = load();
                check(worker(data, 1).skillsLoaded && worker(data, 1).record.skills.empty(), "successful empty read differs from failure");
                check(worker(data, 2).record.skills.size() == 1 && worker(data, 2).record.skills[0].skillId == 1, "inactive global skill excluded");
            });
            test("complete active location chain", [&] {
                const auto data = load();
                check(data.order.location.verified && data.order.location.ancestors == std::vector<std::int64_t>{7,3,1}, "nearest parent through root exact");
                check(data.order.location.campusId == 1 && data.order.location.buildingId == 3, "typed campus and building");
                check(worker(data, 1).serviceArea.verified && worker(data, 1).serviceArea.ancestors.empty() && !worker(data, 1).serviceArea.buildingId, "campus service area never claims same building");
                db.execute("UPDATE workers SET service_area_id=3 WHERE id=1");
                const auto moved = load(); check(worker(moved, 1).serviceArea.verified && worker(moved, 1).serviceArea.buildingId == 3, "actual building coverage");
            });
            test("same campus other building is outside pool", [&] {
                db.execute("UPDATE workers SET service_area_id=4 WHERE id=1");
                const auto data = load(); check(data.workers.size() == 2, "unrelated building cannot be widened by shared campus");
            });
            for (const auto id : {1,3,7,11}) test("inactive location " + std::to_string(id), [&] {
                auto q = db.prepare("UPDATE locations SET active=0 WHERE id=?1");q.bindInt(1,id);q.step();
                rejects(load, "inactive node anywhere blocks unverifiable order scope");
            });
            for (const auto *sql : {
                "UPDATE locations SET parent_id=11 WHERE id=1",
                "UPDATE locations SET parent_id=NULL WHERE id=7",
                "UPDATE locations SET location_type='FLOOR' WHERE id=3",
                "UPDATE locations SET parent_id=1 WHERE id=11",
                "UPDATE locations SET location_type='CAMPUS' WHERE id=7"}) test("malformed hierarchy fails closed", [&] {
                db.execute(sql); rejects(load, "cycle, disconnected root, or invalid type path never becomes verified");
            });
            test("valid AREA and prefix nodes", [&] {
                db.execute("UPDATE locations SET location_type='AREA' WHERE id=11");check(load().order.location.verified, "floor public AREA is valid");
                db.execute("UPDATE locations SET parent_id=1 WHERE id=11");check(load().order.location.verified, "campus public AREA remains compatible with D2/D3");
                db.execute("UPDATE locations SET parent_id=3 WHERE id=11");check(load().order.location.verified, "building public AREA is valid");
                for (const auto id : {1,3,7}) {auto q=db.prepare("UPDATE work_orders SET location_id=?1 WHERE id=1");q.bindInt(1,id);q.step();check(load().order.location.verified,"actual prefix location is complete to campus");}
            });
            test("real load depends solely on ended_at", [&] {
                for (const auto id : {100,101,102}) order(db,id,"COMPLETED");
                assignment(db,100,100,1,{}, {}, {}, {});assignment(db,101,101,1,{}, {}, {}, {});assignment(db,102,102,1,"REFUSED");
                const auto data=load();check(worker(data,1).record.activeTasks==2,"all current tasks count even inconsistent terminal order state; ended old task excluded");
            });
            test("latest twenty after qualification, exact UTC ties and final rework time", [&] {
                for (int i=0;i<30;++i) {order(db,1000+i);assignment(db,10000+i,1000+i,1);rating(db,20000+i,1000+i,4,i%2?"2020-01-02T00:00:00.000Z":"2020-01-02T00:00:00Z");}
                for (int i=0;i<25;++i) {order(db,2000+i);assignment(db,30000+i,2000+i,1,"REFUSED",std::string("not a timestamp"));}
                order(db,2100);assignment(db,31000,2100,1,"COMPLETED",{});
                order(db,2101);assignment(db,31001,2101,1,"COMPLETED",std::string("2020-01-01T00:00:00Z"),{});
                order(db,2102,"CANCELLED");assignment(db,31002,2102,1);
                const auto data=load();const auto &h=worker(data,1).history;
                check(h.efficiency->size()==20&&h.rating->size()==20,"valid older samples not lost behind invalid newer records");
                for(std::size_t i=0;i<20;++i) {
                    check(h.efficiency->at(i).assignmentId==10029-static_cast<std::int64_t>(i),"ended timestamp tie uses descending int64 id");
                    check(h.rating->at(i).evaluationId==20029-static_cast<std::int64_t>(i),"equal UTC .000Z/Z timestamps use descending id, not text order");
                    check(h.efficiency->at(i).targetCompletionMinutes==120&&h.efficiency->at(i).actualMinutes==240.0,"final submitted time, raw duration and actual category target");
                }
            });
            test("final completed owner alone receives rating and efficiency", [&] {
                order(db,3000);assignment(db,40000,3000,1,"TRANSFERRED");assignment(db,40001,3000,2);rating(db,40000,3000,5);
                // Even a corrupt earlier COMPLETED marker must not claim final ownership.
                order(db,3001);assignment(db,40002,3001,1);assignment(db,40003,3001,2);rating(db,40001,3001,3);
                const auto data=load();check(worker(data,1).history.efficiency->empty()&&worker(data,1).history.rating->empty(),"previous transferred/completed owner never gets final rating");
                check(worker(data,2).history.efficiency->size()==2&&worker(data,2).history.rating->size()==2,"final normal completer owns both raw metrics");
            });
            for(int count=1;count<=2;++count)test("real loaded cold-start sample count "+std::to_string(count),[&]{
                db.execute("UPDATE users SET status='ACTIVE' WHERE role='worker'");
                for(int i=0;i<count;++i){order(db,3500+i);assignment(db,45000+i,3500+i,1);rating(db,45000+i,3500+i,5);}
                // Simulate the final submission timestamp being replaced after rework.
                db.execute("UPDATE assignments SET result_submitted_at='2020-01-01T04:30:00Z' WHERE id=45000");
                const auto data=load();const auto result=dispatch::WeightedScoreStrategy{}.recommend(data.order,data.workers);
                const auto &candidate=result.eligible.at(0);
                check(candidate.efficiencySamples==static_cast<std::size_t>(count)&&candidate.ratingSamples==static_cast<std::size_t>(count),"actual SQLite sample counts retained");
                check(candidate.score->components.efficiency==60&&candidate.score->components.rating==60,"only strategy applies frozen cold start to actual one/two samples");
                check(worker(data,1).history.efficiency->back().actualMinutes==270,"final replaced result timestamp is read, not an earlier submission");
            });
            test("fractional minute math does not round or cold-start", [&] {
                order(db,4000);assignment(db,50000,4000,1,"COMPLETED",std::string("2020-02-29T23:59:59.500Z"),std::string("2020-03-01T00:00:30Z"),std::string("2020-03-01T01:00:00Z"));
                const auto data=load();check(std::abs(worker(data,1).history.efficiency->at(0).actualMinutes-30.5/60.0)<1e-12,"UTC leap-day and fraction parsed independent of locale/DST");
            });
            test("zero raw elapsed is legal for algorithm max(1)", [&] {
                order(db,4000);assignment(db,50000,4000,1,"COMPLETED",std::string("2020-01-01T00:00:00Z"),std::string("2020-01-01T00:00:00.000Z"));
                const auto data=load();check(worker(data,1).history.efficiency->at(0).actualMinutes==0,"no fabricated one-minute elapsed in adapter");
            });
            for(const auto *bad:{"not a date","2020-02-30T00:00:00Z","2020-01-01T00:00:00+08:00"}) test("invalid timestamp blocks recommendation",[&]{
                order(db,4000);assignment(db,50000,4000,1,"COMPLETED",std::string(bad));rejects(load,"bad UTC never becomes confirmed zero history");
            });
            test("negative or impossible completion duration blocks recommendation",[&]{
                order(db,4000);assignment(db,50000,4000,1,"COMPLETED",std::string("2020-01-01T06:00:00Z"));rejects(load,"negative elapsed rejected");
            });
            test("malformed evaluation date blocks instead of guessing",[&]{
                order(db,4000);assignment(db,50000,4000,1);rating(db,50000,4000,4,"invalid");rejects(load,"rating query parse error not cold start");
            });
            for(const auto *sql:{"UPDATE work_orders SET approved_urgency=NULL WHERE id=1","UPDATE skills SET active=0 WHERE id=1","UPDATE fault_categories SET active=0 WHERE id=1"}) test("missing required reviewed configuration is explicit failure",[&]{db.execute(sql);rejects(load,"missing configuration is not a default");});
            for(const auto *table:{"worker_skills","assignments","evaluations","locations"}) test("read failure is never loaded empty "+std::string(table),[&]{
                std::string denied(table);sqlite3_set_authorizer(db.nativeHandle(),deny,&denied);
                bool failed=false;try{load();}catch(const std::exception &){failed=true;}
                sqlite3_set_authorizer(db.nativeHandle(),nullptr,nullptr);check(failed,"failed SQL blocks complete result");
            });
            test("64-bit worker and order IDs preserved",[&]{
                constexpr std::int64_t big=5000000000LL;
                auto user=db.prepare("INSERT INTO users(id,username,password_hash,display_name,role,status) VALUES(?1,'large_worker','!DISABLED!','Fictional','worker','DISABLED')");user.bindInt(1,big);user.step();
                auto person=db.prepare("INSERT INTO workers(id,user_id,worker_no,service_area_id) VALUES(?1,?1,'LARGE',1)");person.bindInt(1,big);person.step();
                order(db,big,"PENDING_DISPATCH");const auto data=DispatchRepository(db).load(big);
                check(data.order.id==big&&worker(data,big).record.userId==big,"no narrowing at SQL/adapter boundary");
            });
            test("production scoring consumes real adapter data and verified zero samples",[&]{
                db.execute("UPDATE users SET status='ACTIVE' WHERE role='worker'");
                const auto data=load();const auto result=dispatch::WeightedScoreStrategy{}.recommend(data.order,data.workers);
                check(result.eligible.size()==1&&result.eligible[0].workerId==1,"actual qualification excludes uncertified/off-duty profiles");
                const auto &c=result.eligible[0];check(c.score&&c.score->components.skill==100&&c.score->components.area==70&&c.score->components.load==100&&c.score->components.efficiency==60&&c.score->components.rating==60&&c.score->components.duty==0,"real six component cold-start values only computed in strategy");
                check(c.score->total==81&&c.rank==1&&c.efficiencySamples==0&&c.ratingSamples==0,"independent frozen-weight recomputation 35+14+20+6+6=81");
            });
            test("six bounded batch queries and whole database read-only",[&]{
                const auto before=fingerprint(db);QueryTrace state;sqlite3_trace_v2(db.nativeHandle(),SQLITE_TRACE_STMT,trace,&state);
                for(int i=0;i<3;++i)load();sqlite3_trace_v2(db.nativeHandle(),0,nullptr,nullptr);
                check(state.selects==18&&state.readonly,"six SELECTs per load, no per-worker/skill/location N+1 or writes");
                check(before==fingerprint(db),"all tables and every column unchanged across repeated load");
            });
            test("not-found remains not-found",[&]{bool failed=false;try{DispatchRepository(db).load(std::numeric_limits<std::int64_t>::max());}catch(const WorkOrderError &e){failed=e.status==404;}check(failed,"missing order is not empty candidate success");});
            check(db.scalarInt("SELECT count(*) FROM pragma_foreign_key_check")==0,"foreign_key_check empty");
            auto integrity=db.prepare("PRAGMA integrity_check");check(integrity.step()&&integrity.columnText(0)=="ok","integrity_check ok");
        }
        std::filesystem::remove_all(root);
        std::cout << "PASS DispatchRepository: " << cases << " cases, " << checks << " assertions; real migrated/seeded isolated SQLite; six batch SELECTs per load\n";
        return 0;
    } catch(const std::exception &e) {
        std::cerr << "FAIL DispatchRepository: " << e.what() << '\n';
        std::filesystem::remove_all(root);return 1;
    }
}
