#include "infrastructure/DatabaseMigration.h"

#include <sqlite3.h>

#include <algorithm>
#include <cctype>
#include <fstream>
#include <iostream>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace campusfix
{
namespace
{
using Database = std::unique_ptr<sqlite3, decltype(&sqlite3_close)>;
using Statement = std::unique_ptr<sqlite3_stmt, decltype(&sqlite3_finalize)>;

void execute(sqlite3 *db, const std::string &sql)
{
    char *error = nullptr;
    if (sqlite3_exec(db, sql.c_str(), nullptr, nullptr, &error) != SQLITE_OK)
    {
        const std::string message = error != nullptr ? error : sqlite3_errmsg(db);
        sqlite3_free(error);
        throw std::runtime_error(message);
    }
}

Statement prepare(sqlite3 *db, const char *sql)
{
    sqlite3_stmt *raw = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &raw, nullptr) != SQLITE_OK)
    {
        throw std::runtime_error(sqlite3_errmsg(db));
    }
    return Statement(raw, &sqlite3_finalize);
}

int integerQuery(sqlite3 *db, const char *sql)
{
    const auto statement = prepare(db, sql);
    if (sqlite3_step(statement.get()) != SQLITE_ROW)
    {
        throw std::runtime_error(sqlite3_errmsg(db));
    }
    return sqlite3_column_int(statement.get(), 0);
}

std::string readSql(const std::filesystem::path &path)
{
    std::ifstream input(path, std::ios::binary);
    if (!input)
    {
        throw std::runtime_error("Cannot read migration: " + path.filename().string());
    }
    return std::string(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
}

int migrationVersion(const std::filesystem::path &path)
{
    const std::string name = path.filename().string();
    const auto separator = name.find('_');
    if (separator == std::string::npos || separator == 0 || path.extension() != ".sql" ||
        !std::all_of(name.begin(), name.begin() + separator,
                     [](unsigned char value) { return std::isdigit(value) != 0; }))
    {
        throw std::runtime_error("Invalid migration filename: " + name);
    }
    const int version = std::stoi(name.substr(0, separator));
    if (version <= 0)
    {
        throw std::runtime_error("Migration version must be positive: " + name);
    }
    return version;
}

std::map<int, std::filesystem::path> findMigrations(const std::filesystem::path &directory)
{
    if (!std::filesystem::is_directory(directory))
    {
        throw std::runtime_error("Migration directory does not exist");
    }
    std::map<int, std::filesystem::path> migrations;
    for (const auto &entry : std::filesystem::directory_iterator(directory))
    {
        if (!entry.is_regular_file() || entry.path().extension() != ".sql")
        {
            continue;
        }
        const int version = migrationVersion(entry.path());
        if (!migrations.emplace(version, entry.path()).second)
        {
            throw std::runtime_error("Duplicate migration version: " + std::to_string(version));
        }
    }
    if (migrations.empty())
    {
        throw std::runtime_error("No SQL migrations found");
    }
    return migrations;
}

std::map<int, std::string> appliedMigrations(sqlite3 *db)
{
    auto statement = prepare(db, "SELECT version, name FROM schema_migrations ORDER BY version");
    std::map<int, std::string> applied;
    int result = SQLITE_ROW;
    while ((result = sqlite3_step(statement.get())) == SQLITE_ROW)
    {
        applied.emplace(sqlite3_column_int(statement.get(), 0),
                        reinterpret_cast<const char *>(sqlite3_column_text(statement.get(), 1)));
    }
    if (result != SQLITE_DONE)
    {
        throw std::runtime_error(sqlite3_errmsg(db));
    }
    return applied;
}
}  // namespace

void applyMigrations(const std::filesystem::path &databasePath,
                     const std::filesystem::path &migrationsDirectory)
{
    if (sqlite3_libversion_number() < 3035000)
    {
        throw std::runtime_error("SQLite 3.35 or newer is required");
    }
    const auto migrations = findMigrations(migrationsDirectory);
    const auto parent = databasePath.has_parent_path() ? databasePath.parent_path() :
                                                         std::filesystem::current_path();
    if (!std::filesystem::is_directory(parent))
    {
        throw std::runtime_error("Database parent directory does not exist");
    }

    sqlite3 *raw = nullptr;
    const std::string utf8Path = databasePath.u8string();
    const int openResult = sqlite3_open_v2(utf8Path.c_str(), &raw,
                                           SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, nullptr);
    Database db(raw, &sqlite3_close);
    if (openResult != SQLITE_OK)
    {
        throw std::runtime_error("Cannot open SQLite database: " +
                                 std::string(raw != nullptr ? sqlite3_errmsg(raw) : "unknown error"));
    }
    sqlite3_extended_result_codes(db.get(), 1);
    if (sqlite3_busy_timeout(db.get(), 5000) != SQLITE_OK)
    {
        throw std::runtime_error("Cannot configure SQLite busy_timeout");
    }
    // PRAGMA foreign_keys is per-connection and has no effect inside a transaction.
    execute(db.get(), "PRAGMA foreign_keys=ON");
    if (integerQuery(db.get(), "PRAGMA foreign_keys") != 1)
    {
        throw std::runtime_error("SQLite foreign key enforcement is unavailable");
    }

    execute(db.get(), "BEGIN IMMEDIATE");
    try
    {
        std::vector<std::string> newlyApplied;
        std::vector<std::string> alreadyApplied;
        execute(db.get(),
                "CREATE TABLE IF NOT EXISTS schema_migrations ("
                "version INTEGER PRIMARY KEY, name TEXT NOT NULL, "
                "applied_at TEXT NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%fZ','now'))) ");
        const auto applied = appliedMigrations(db.get());
        for (const auto &[version, name] : applied)
        {
            const auto found = migrations.find(version);
            if (found == migrations.end() || found->second.filename().string() != name)
            {
                throw std::runtime_error("Applied migration is missing or renamed: " + name);
            }
        }
        for (const auto &[version, path] : migrations)
        {
            const std::string name = path.filename().string();
            if (applied.count(version) != 0)
            {
                alreadyApplied.push_back(name);
                continue;
            }
            const std::string sql = readSql(path);
            try
            {
                execute(db.get(), sql);
            }
            catch (const std::exception &error)
            {
                throw std::runtime_error("Migration " + name + " failed: " + error.what());
            }
            auto statement = prepare(db.get(),
                "INSERT INTO schema_migrations(version, name) VALUES (?1, ?2)");
            sqlite3_bind_int(statement.get(), 1, version);
            sqlite3_bind_text(statement.get(), 2, name.c_str(), -1, SQLITE_TRANSIENT);
            if (sqlite3_step(statement.get()) != SQLITE_DONE)
            {
                throw std::runtime_error("Cannot record migration " + name + ": " + sqlite3_errmsg(db.get()));
            }
            newlyApplied.push_back(name);
        }
        execute(db.get(), "COMMIT");
        for (const auto &name : alreadyApplied)
        {
            std::cout << "SKIP " << name << " (already applied)\n";
        }
        for (const auto &name : newlyApplied)
        {
            std::cout << "APPLY " << name << '\n';
        }
    }
    catch (...)
    {
        sqlite3_exec(db.get(), "ROLLBACK", nullptr, nullptr, nullptr);
        throw;
    }
}
}  // namespace campusfix
