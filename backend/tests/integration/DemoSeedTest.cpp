#include "infrastructure/DatabaseMigration.h"
#include "infrastructure/DemoSeed.h"
#include "infrastructure/SqliteConnection.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>

#ifndef CAMPUSFIX_MIGRATIONS_DIR
#error CAMPUSFIX_MIGRATIONS_DIR is required
#endif
#ifndef CAMPUSFIX_SEED_FILE
#error CAMPUSFIX_SEED_FILE is required
#endif

namespace
{
void require(bool condition, const std::string &message)
{
    if (!condition)
    {
        throw std::runtime_error(message);
    }
}

std::int64_t count(campusfix::SqliteConnection &db, const char *table)
{
    // Table names below are compile-time test constants, not user input.
    return db.scalarInt(std::string("SELECT COUNT(*) FROM ") + table);
}

void checkCounts(campusfix::SqliteConnection &db)
{
    require(count(db, "users") == 11, "Expected 11 demo users");
    require(count(db, "locations") == 14, "Expected 14 locations");
    require(count(db, "skills") == 3, "Expected three skills");
    require(count(db, "fault_categories") == 3, "Expected three categories");
    require(count(db, "workers") == 6, "Expected six worker profiles");
    require(count(db, "worker_skills") == 7, "Expected seven worker skills");
    require(count(db, "management_grants") == 4, "Expected four grants");
    require(count(db, "work_orders") == 4, "Expected four demo work orders");
    require(count(db, "status_logs") == 8, "Expected creation and submit logs for each order");
}

void checkPragma(campusfix::SqliteConnection &db, const char *sql,
                 const std::string &expected)
{
    auto query = db.prepare(sql);
    require(query.step() && query.columnText(0) == expected && !query.step(),
            std::string(sql) + " did not return " + expected);
}
}  // namespace

int main()
{
    const auto nonce = std::chrono::steady_clock::now().time_since_epoch().count();
    const auto root = std::filesystem::temp_directory_path() /
                      ("campusfix-w3d2-test-" + std::to_string(nonce));
    try
    {
        std::filesystem::create_directories(root);
        const auto migrations = std::filesystem::path(CAMPUSFIX_MIGRATIONS_DIR);
        const auto seed = std::filesystem::path(CAMPUSFIX_SEED_FILE);
        const auto databasePath = root / "demo.sqlite3";
        bool rejectedBeforeMigration = false;
        try
        {
            campusfix::applyDemoSeed(databasePath, seed);
        }
        catch (const std::exception &)
        {
            rejectedBeforeMigration = true;
        }
        require(rejectedBeforeMigration && !std::filesystem::exists(databasePath),
                "Seed before migration must fail without creating database");
        std::cout << "PASS seed requires existing migrated database\n";

        campusfix::applyMigrations(databasePath, migrations);
        campusfix::applyDemoSeed(databasePath, seed);
        {
            campusfix::SqliteConnection db(databasePath);
            require(db.scalarInt("PRAGMA foreign_keys") == 1, "foreign_keys must be ON per connection");
            require(db.scalarInt("PRAGMA busy_timeout") == 5000, "busy_timeout must be 5000 ms");
            checkCounts(db);
            require(db.scalarInt("SELECT COUNT(*) FROM users WHERE status='DISABLED' "
                                 "AND password_hash='!UNSET_DEMO_CREDENTIAL!'") == 11,
                    "Demo users must remain unloginable placeholders");
            require(db.scalarInt("SELECT COUNT(*) FROM users WHERE role='sysadmin'") == 1 &&
                    db.scalarInt("SELECT COUNT(*) FROM users WHERE role='dispatcher'") == 1 &&
                    db.scalarInt("SELECT COUNT(*) FROM users WHERE role='reporter'") == 3 &&
                    db.scalarInt("SELECT COUNT(*) FROM users WHERE role='worker'") == 6,
                    "Four role distribution differs");
            require(db.scalarInt("SELECT COUNT(*) FROM locations WHERE location_type='CAMPUS'") == 2 &&
                    db.scalarInt("SELECT COUNT(*) FROM locations WHERE location_type='BUILDING'") == 4 &&
                    db.scalarInt("SELECT COUNT(*) FROM locations WHERE location_type='FLOOR'") == 4 &&
                    db.scalarInt("SELECT COUNT(*) FROM locations WHERE location_type='ROOM'") == 4,
                    "Location hierarchy differs");
            require(db.scalarInt("SELECT COUNT(*) FROM locations AS child LEFT JOIN locations AS parent "
                                 "ON child.parent_id=parent.id WHERE child.location_type!='CAMPUS' "
                                 "AND parent.id IS NULL") == 0,
                    "Non-campus location has no parent");
            require(db.scalarInt("SELECT COUNT(*) FROM workers w JOIN users u ON w.user_id=u.id "
                                 "WHERE u.role='worker'") == 6,
                    "Worker profiles must belong to worker accounts");
            require(db.scalarInt("SELECT COUNT(*) FROM worker_skills ws JOIN workers w ON ws.worker_id=w.id "
                                 "JOIN skills s ON ws.skill_id=s.id") == 7,
                    "Worker skill references are incomplete");
            require(db.scalarInt("SELECT COUNT(*) FROM management_grants g JOIN users u ON g.user_id=u.id "
                                 "JOIN locations l ON g.scope_location_id=l.id "
                                 "WHERE u.username='demo_dispatcher' AND g.capability='AREA_SCOPE' "
                                 "AND l.code='DEMO-CAMPUS-A'") == 1,
                    "Dispatcher AREA_SCOPE is incorrect");
            require(db.scalarInt("SELECT COUNT(*) FROM management_grants g JOIN users u ON g.user_id=u.id "
                                 "JOIN locations l ON g.scope_location_id=l.id "
                                 "WHERE u.username='demo_sysadmin' AND l.code='DEMO-CAMPUS-B' "
                                 "AND g.capability IN ('BUSINESS_READ','INTERNAL_NOTE_READ','STATS_VIEW')") == 3,
                    "Sysadmin grants are incorrect");
            require(db.scalarInt("SELECT COUNT(*) FROM management_grants g JOIN users u ON g.user_id=u.id "
                                 "WHERE u.username='demo_sysadmin' AND g.capability IN ('REVIEW','DISPATCH')") == 0,
                    "Sysadmin must not have implicit review/dispatch grants");
            require(db.scalarInt("SELECT COUNT(*) FROM work_orders WHERE status='PENDING_REVIEW' "
                                 "AND order_no LIKE 'WX20200101%' AND submitted_at IS NOT NULL") == 4,
                    "Demo orders should be formally submitted examples");
            require(db.scalarInt("SELECT COUNT(*) FROM work_orders WHERE created_at <= submitted_at "
                                 "AND submitted_at=updated_at") == 4,
                    "Demo order timestamps are inconsistent");
            require(db.scalarInt("SELECT COUNT(*) FROM work_orders w JOIN users u ON w.reporter_id=u.id "
                                 "WHERE u.username='demo_worker01'") == 1,
                    "Worker self-report example is missing");
            require(db.scalarInt("SELECT COUNT(*) FROM status_logs WHERE action='ORDER_SUBMITTED'") == 4,
                    "Demo order timelines are incomplete");
            require(db.scalarInt("SELECT COUNT(*) FROM status_logs WHERE action='ORDER_CREATED' "
                                 "AND from_status IS NULL AND to_status='DRAFT'") == 4,
                    "Demo order creation logs are incomplete");
            checkPragma(db, "PRAGMA integrity_check", "ok");
            require(db.scalarInt("SELECT COUNT(*) FROM pragma_foreign_key_check") == 0,
                    "foreign_key_check found violations");
            std::cout << "PASS seed counts, links, roles, scopes, integrity and foreign keys\n";

            auto safeQuery = db.prepare("SELECT COUNT(*) FROM users WHERE username=?1");
            safeQuery.bindText(1, "demo_reporter01' OR 1=1 --");
            require(safeQuery.step() && safeQuery.columnInt(0) == 0,
                    "Bound text unexpectedly changed SQL meaning");
            auto exactQuery = db.prepare("SELECT id FROM users WHERE username=?1");
            exactQuery.bindText(1, "demo_reporter01");
            require(exactQuery.step() && exactQuery.columnInt(0) > 0,
                    "Parameterized lookup failed");
            auto boundValues = db.prepare("SELECT ?1,?2,?3");
            boundValues.bindInt(1, 42);
            boundValues.bindText(2, "fictional");
            boundValues.bindNull(3);
            require(boundValues.step() && boundValues.columnInt(0) == 42 &&
                    boundValues.columnText(1) == "fictional" && boundValues.columnIsNull(2),
                    "Integer/text/NULL binding failed");
            std::cout << "PASS parameterized statement binding\n";

            {
                campusfix::SqliteTransaction rollback(db);
                db.execute("INSERT INTO skills(code,name) VALUES ('DEMO-TX-ROLLBACK','Temporary')");
            }
            require(count(db, "skills") == 3, "RAII transaction did not roll back");
            std::cout << "PASS uncommitted transaction rolled back on scope exit\n";

            db.execute("UPDATE users SET display_name='User Edited Name' "
                       "WHERE username='demo_reporter01'");
            db.execute("UPDATE fault_categories SET target_completion_minutes=321 "
                       "WHERE code='DEMO-SOCKET'");
        }

        campusfix::applyDemoSeed(databasePath, seed);
        {
            campusfix::SqliteConnection db(databasePath);
            checkCounts(db);
            require(db.scalarInt("SELECT COUNT(*) FROM users WHERE username='demo_reporter01' "
                                 "AND display_name='User Edited Name'") == 1,
                    "Repeat seed overwrote edited user");
            require(db.scalarInt("SELECT target_completion_minutes FROM fault_categories "
                                 "WHERE code='DEMO-SOCKET'") == 321,
                    "Repeat seed overwrote edited category");
            std::cout << "PASS repeat seed made no duplicates and preserved edits\n";
        }

        const auto badSeed = root / "bad-seed.sql";
        {
            std::ofstream file(badSeed);
            file << "INSERT INTO skills(code,name) VALUES ('DEMO-ROLLBACK','Must not persist');\n"
                    "INSERT INTO workers(user_id,worker_no,service_area_id) VALUES (999999,'BAD-WORKER',1);\n";
        }
        bool failed = false;
        try
        {
            campusfix::applyDemoSeed(databasePath, badSeed);
        }
        catch (const std::exception &)
        {
            failed = true;
        }
        require(failed, "Invalid seed unexpectedly succeeded");
        {
            campusfix::SqliteConnection db(databasePath);
            require(count(db, "skills") == 3 &&
                    db.scalarInt("SELECT COUNT(*) FROM skills WHERE code='DEMO-ROLLBACK'") == 0,
                    "Failed seed left partial data");
            checkPragma(db, "PRAGMA integrity_check", "ok");
            require(db.scalarInt("SELECT COUNT(*) FROM pragma_foreign_key_check") == 0,
                    "Foreign key violation after failed seed");
        }
        std::cout << "PASS failed seed rolled back all writes\n";
        std::filesystem::remove_all(root);
        std::cout << "PASS all W3-D2 demo seed tests\n";
        return 0;
    }
    catch (const std::exception &error)
    {
        std::cerr << "FAIL W3-D2 demo seed test: " << error.what() << '\n';
        std::cerr << "Synthetic test fixture retained at: " << root.string() << '\n';
        return 1;
    }
}
