#include "infrastructure/DatabaseMigration.h"

#include <sqlite3.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#ifndef CAMPUSFIX_MIGRATIONS_DIR
#error CAMPUSFIX_MIGRATIONS_DIR is required
#endif

namespace
{
using Database = std::unique_ptr<sqlite3, decltype(&sqlite3_close)>;

void require(bool condition, const std::string &message)
{
    if (!condition)
    {
        throw std::runtime_error(message);
    }
}

Database open(const std::filesystem::path &path)
{
    sqlite3 *raw = nullptr;
    const auto result = sqlite3_open_v2(path.u8string().c_str(), &raw, SQLITE_OPEN_READWRITE, nullptr);
    Database db(raw, &sqlite3_close);
    require(result == SQLITE_OK, "Cannot open test database");
    require(sqlite3_busy_timeout(db.get(), 5000) == SQLITE_OK, "busy_timeout failed");
    char *error = nullptr;
    require(sqlite3_exec(db.get(), "PRAGMA foreign_keys=ON", nullptr, nullptr, &error) == SQLITE_OK,
            "Cannot enable foreign keys");
    sqlite3_free(error);
    return db;
}

void execute(sqlite3 *db, const std::string &sql)
{
    char *error = nullptr;
    const int result = sqlite3_exec(db, sql.c_str(), nullptr, nullptr, &error);
    if (result != SQLITE_OK)
    {
        const std::string message = error != nullptr ? error : sqlite3_errmsg(db);
        sqlite3_free(error);
        throw std::runtime_error(message + " in: " + sql);
    }
}

void rejected(sqlite3 *db, const std::string &sql, int expectedPrimaryCode, const std::string &label)
{
    char *error = nullptr;
    const int result = sqlite3_exec(db, sql.c_str(), nullptr, nullptr, &error);
    sqlite3_free(error);
    require((result & 0xff) == expectedPrimaryCode, label + " was not rejected as expected");
    std::cout << "PASS rejected " << label << '\n';
}

int scalar(sqlite3 *db, const std::string &sql)
{
    sqlite3_stmt *raw = nullptr;
    require(sqlite3_prepare_v2(db, sql.c_str(), -1, &raw, nullptr) == SQLITE_OK,
            "Cannot prepare scalar query");
    const std::unique_ptr<sqlite3_stmt, decltype(&sqlite3_finalize)> stmt(raw, &sqlite3_finalize);
    require(sqlite3_step(stmt.get()) == SQLITE_ROW, "Scalar query returned no row");
    return sqlite3_column_int(stmt.get(), 0);
}

void checkPragma(sqlite3 *db, const char *sql, const std::string &expected)
{
    sqlite3_stmt *raw = nullptr;
    require(sqlite3_prepare_v2(db, sql, -1, &raw, nullptr) == SQLITE_OK, "PRAGMA prepare failed");
    const std::unique_ptr<sqlite3_stmt, decltype(&sqlite3_finalize)> stmt(raw, &sqlite3_finalize);
    require(sqlite3_step(stmt.get()) == SQLITE_ROW, "PRAGMA returned no row");
    const unsigned char *value = sqlite3_column_text(stmt.get(), 0);
    require(value != nullptr && reinterpret_cast<const char *>(value) == expected, "PRAGMA unexpected result");
    require(sqlite3_step(stmt.get()) == SQLITE_DONE, "PRAGMA returned unexpected extra rows");
    std::cout << "PASS " << sql << " => " << expected << '\n';
}

void checkForeignKeyList(sqlite3 *db, const std::string &table, int expectedCount)
{
    require(scalar(db, "SELECT COUNT(*) FROM pragma_foreign_key_list('" + table +
                      "') WHERE on_delete='RESTRICT'") == expectedCount,
            table + " RESTRICT foreign keys differ from frozen design");
}
}  // namespace

int main()
{
    const auto unique = std::chrono::steady_clock::now().time_since_epoch().count();
    const auto root = std::filesystem::temp_directory_path() /
                      ("campusfix-schema-test-" + std::to_string(unique));
    try
    {
        std::filesystem::create_directories(root);
        const auto migrationDir = std::filesystem::path(CAMPUSFIX_MIGRATIONS_DIR);
        const auto databasePath = root / "schema.sqlite3";

        // Failure after a valid migration must roll back both DDL and version records.
        const auto badDir = root / "bad-migrations";
        std::filesystem::create_directory(badDir);
        std::filesystem::copy_file(migrationDir / "001_initial_schema.sql",
                                   badDir / "001_initial_schema.sql");
        {
            std::ofstream badSql(badDir / "002_invalid.sql");
            badSql << "CREATE TABLE must_rollback (id INTEGER PRIMARY KEY);\n"
                      "INSERT INTO no_such_table VALUES (1);\n";
        }
        bool failed = false;
        try
        {
            campusfix::applyMigrations(root / "rollback.sqlite3", badDir);
        }
        catch (const std::exception &)
        {
            failed = true;
        }
        require(failed, "Invalid migration unexpectedly succeeded");
        {
            auto rollbackDb = open(root / "rollback.sqlite3");
            require(scalar(rollbackDb.get(), "SELECT COUNT(*) FROM sqlite_master WHERE type='table' "
                                            "AND name NOT LIKE 'sqlite_%'") == 0,
                    "Failed migration left partial tables");
        }
        std::cout << "PASS failed migration rolled back DDL and version metadata\n";

        campusfix::applyMigrations(databasePath, migrationDir);
        auto db = open(databasePath);
        require(scalar(db.get(), "PRAGMA foreign_keys") == 1, "Foreign keys disabled on test connection");
        require(scalar(db.get(), "PRAGMA busy_timeout") == 5000, "busy_timeout differs");
        const std::vector<std::string> tables = {
            "users", "auth_sessions", "management_grants", "locations", "skills",
            "fault_categories", "workers", "worker_skills", "order_number_sequences",
            "work_orders", "attachments", "assignments", "transfer_requests",
            "status_logs", "dispatch_records", "evaluations"};
        for (const auto &table : tables)
        {
            require(scalar(db.get(), "SELECT COUNT(*) FROM sqlite_master WHERE type='table' AND name='" +
                          table + "'") == 1, "Missing table " + table);
        }
        require(scalar(db.get(), "SELECT COUNT(*) FROM sqlite_master WHERE type='table' "
                                 "AND name NOT LIKE 'sqlite_%'") == 17,
                "Expected 16 business tables plus schema_migrations");
        require(scalar(db.get(), "SELECT COUNT(*) FROM sqlite_master WHERE type='index' "
                                 "AND name IN ('uq_management_grants_active','uq_assignments_active_order',"
                                 "'uq_transfer_requests_pending_assignment','uq_status_logs_arrived_assignment')") == 4,
                "Missing critical partial unique indexes");
        require(scalar(db.get(), "SELECT COUNT(*) FROM sqlite_master WHERE type='index' "
                                 "AND name NOT LIKE 'sqlite_%'") >= 25,
                "Missing ordinary indexes");
        checkForeignKeyList(db.get(), "work_orders", 3);
        checkForeignKeyList(db.get(), "assignments", 3);
        checkForeignKeyList(db.get(), "transfer_requests", 4);
        checkForeignKeyList(db.get(), "status_logs", 3);
        require(scalar(db.get(), "SELECT COUNT(*) FROM pragma_foreign_key_list('auth_sessions') "
                                 "WHERE on_delete='CASCADE'") == 1,
                "auth_sessions user FK must cascade");
        std::cout << "PASS 16 business tables, migration metadata, indexes, FK policies\n";

        execute(db.get(), "INSERT INTO users(id,username,password_hash,display_name,role) VALUES "
                          "(1,'fake_reporter','fake-hash','Fictional Reporter','reporter'),"
                          "(2,'fake_worker','fake-hash','Fictional Worker','worker'),"
                          "(3,'fake_dispatcher','fake-hash','Fictional Dispatcher','dispatcher')");
        execute(db.get(), "INSERT INTO locations(id,code,name,location_type) "
                          "VALUES (1,'TEST-CAMPUS','Fictional Campus','CAMPUS')");
        execute(db.get(), "INSERT INTO skills(id,code,name) VALUES (1,'TEST-SKILL','Fictional Skill')");
        execute(db.get(), "INSERT INTO fault_categories(id,code,name,required_skill_id,target_completion_minutes) "
                          "VALUES (1,'TEST-CAT','Fictional Category',1,60)");
        execute(db.get(), "INSERT INTO workers(id,user_id,worker_no,service_area_id) "
                          "VALUES (1,2,'TEST-WORKER',1)");
        execute(db.get(), "INSERT INTO work_orders(id,reporter_id,category_id,location_id,title,description,"
                          "contact_name,contact_phone) VALUES "
                          "(1,1,1,1,'Fake order one','Test-only description','Fictional','00000000000'),"
                          "(2,1,1,1,'Fake order two','Test-only description','Fictional','00000000000')");
        require(scalar(db.get(), "SELECT COUNT(*) FROM work_orders WHERE order_no IS NULL") == 2,
                "Multiple DRAFT NULL order numbers not supported");
        rejected(db.get(), "INSERT INTO work_orders(reporter_id,category_id,location_id,title,description,"
                           "contact_name,contact_phone) VALUES "
                           "(999,1,1,'Invalid','Test','Fictional','000')", SQLITE_CONSTRAINT,
                 "invalid reporter FK");
        rejected(db.get(), "INSERT INTO users(username,password_hash,display_name,role) "
                           "VALUES ('fake_reporter','fake','Duplicate','reporter')", SQLITE_CONSTRAINT,
                 "unique username");
        rejected(db.get(), "INSERT INTO users(username,password_hash,display_name,role) "
                           "VALUES ('fake_bad_role','fake','Invalid','superuser')", SQLITE_CONSTRAINT,
                 "role CHECK");
        rejected(db.get(), "UPDATE work_orders SET status='UNKNOWN' WHERE id=1", SQLITE_CONSTRAINT,
                 "order status CHECK");
        rejected(db.get(), "INSERT INTO fault_categories(code,name,required_skill_id,target_completion_minutes) "
                           "VALUES ('BAD-TARGET','Invalid',1,0)", SQLITE_CONSTRAINT,
                 "target completion CHECK");
        rejected(db.get(), "INSERT INTO worker_skills(worker_id,skill_id,level) VALUES (1,1,6)",
                 SQLITE_CONSTRAINT, "skill level CHECK");
        rejected(db.get(), "INSERT INTO management_grants(user_id,capability,granted_by) "
                           "VALUES (3,'SYSTEM_MANAGE',3)", SQLITE_CONSTRAINT, "capability CHECK");
        execute(db.get(), "INSERT INTO management_grants(user_id,capability,granted_by) "
                          "VALUES (3,'AREA_SCOPE',3)");
        rejected(db.get(), "INSERT INTO management_grants(user_id,capability,granted_by) "
                           "VALUES (3,'AREA_SCOPE',3)", SQLITE_CONSTRAINT,
                 "active global grant uniqueness");
        execute(db.get(), "UPDATE work_orders SET order_no='WX202610230001',status='PENDING_DISPATCH' WHERE id=1");
        rejected(db.get(), "UPDATE work_orders SET order_no='WX202610230001' WHERE id=2",
                 SQLITE_CONSTRAINT, "unique non-NULL order number");
        rejected(db.get(), "INSERT INTO order_number_sequences(business_date,last_value) "
                           "VALUES ('20261023',10000)", SQLITE_CONSTRAINT, "number sequence limit");
        execute(db.get(), "INSERT INTO assignments(id,order_id,worker_id,assigned_by) VALUES (1,1,1,3)");
        rejected(db.get(), "INSERT INTO assignments(order_id,worker_id,assigned_by) VALUES (1,1,3)",
                 SQLITE_CONSTRAINT, "single active assignment per order");
        execute(db.get(), "UPDATE assignments SET ended_at='2026-10-23T00:00:00.000Z',"
                          "end_reason='REFUSED' WHERE id=1");
        execute(db.get(), "INSERT INTO assignments(id,order_id,worker_id,assigned_by) VALUES (2,1,1,3)");
        require(scalar(db.get(), "SELECT COUNT(*) FROM assignments WHERE order_id=1 AND ended_at IS NULL") == 1,
                "Ended assignment did not permit one new active assignment");
        execute(db.get(), "INSERT INTO transfer_requests(order_id,assignment_id,requested_by,reason,order_version) "
                          "VALUES (1,2,2,'Fictional request',1)");
        rejected(db.get(), "INSERT INTO transfer_requests(order_id,assignment_id,requested_by,reason,order_version) "
                           "VALUES (1,2,2,'Duplicate',1)", SQLITE_CONSTRAINT,
                 "single pending transfer per assignment");
        execute(db.get(), "INSERT INTO status_logs(order_id,actor_id,assignment_id,action,to_status,request_id) "
                          "VALUES (1,2,2,'WORKER_ARRIVED','IN_PROGRESS','fake-request-1')");
        rejected(db.get(), "INSERT INTO status_logs(order_id,actor_id,assignment_id,action,to_status,request_id) "
                           "VALUES (1,2,2,'WORKER_ARRIVED','IN_PROGRESS','fake-request-2')",
                 SQLITE_CONSTRAINT, "single arrival per assignment");
        execute(db.get(), "INSERT INTO evaluations(order_id,reporter_id,score) VALUES (1,1,5)");
        rejected(db.get(), "INSERT INTO evaluations(order_id,reporter_id,score) VALUES (2,1,6)",
                 SQLITE_CONSTRAINT, "score CHECK");
        rejected(db.get(), "INSERT INTO evaluations(order_id,reporter_id,score) VALUES (1,1,4)",
                 SQLITE_CONSTRAINT, "unique evaluation per order");
        rejected(db.get(), "DELETE FROM users WHERE id=1", SQLITE_CONSTRAINT,
                 "historical user deletion RESTRICT");
        rejected(db.get(), "DELETE FROM locations WHERE id=1", SQLITE_CONSTRAINT,
                 "referenced location deletion RESTRICT");
        checkPragma(db.get(), "PRAGMA integrity_check", "ok");
        require(scalar(db.get(), "SELECT COUNT(*) FROM pragma_foreign_key_check") == 0,
                "foreign_key_check found violations");
        std::cout << "PASS PRAGMA foreign_key_check => 0 rows\n";
        db.reset();

        campusfix::applyMigrations(databasePath, migrationDir);
        auto repeatedDb = open(databasePath);
        require(scalar(repeatedDb.get(), "SELECT COUNT(*) FROM schema_migrations") == 1,
                "Repeat migration duplicated version metadata");
        require(scalar(repeatedDb.get(), "SELECT COUNT(*) FROM work_orders") == 2,
                "Repeat migration damaged existing data");
        std::cout << "PASS repeat migration preserved schema and fixture data\n";
        repeatedDb.reset();
        std::filesystem::remove_all(root);
        std::cout << "PASS all W3-D1 schema migration tests\n";
        return 0;
    }
    catch (const std::exception &error)
    {
        std::cerr << "FAIL W3-D1 schema migration test: " << error.what() << '\n';
        // Retain failed fixture directory for diagnosis; it contains synthetic data only.
        std::cerr << "Test fixture: " << root.string() << '\n';
        return 1;
    }
}
