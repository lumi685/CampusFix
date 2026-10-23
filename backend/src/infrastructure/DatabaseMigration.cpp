#include "infrastructure/DatabaseMigration.h"
#include "infrastructure/SqliteConnection.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <iostream>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>

namespace campusfix
{
namespace
{
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

std::map<int, std::string> appliedMigrations(SqliteConnection &db)
{
    auto statement = db.prepare("SELECT version, name FROM schema_migrations ORDER BY version");
    std::map<int, std::string> applied;
    while (statement.step())
    {
        applied.emplace(static_cast<int>(statement.columnInt(0)), statement.columnText(1));
    }
    return applied;
}
}  // namespace

void applyMigrations(const std::filesystem::path &databasePath,
                     const std::filesystem::path &migrationsDirectory)
{
    const auto migrations = findMigrations(migrationsDirectory);
    const auto parent = databasePath.has_parent_path() ? databasePath.parent_path() :
                                                         std::filesystem::current_path();
    if (!std::filesystem::is_directory(parent))
    {
        throw std::runtime_error("Database parent directory does not exist");
    }

    SqliteConnection db(databasePath, true);
    {
        SqliteTransaction transaction(db);
        std::vector<std::string> newlyApplied;
        std::vector<std::string> alreadyApplied;
        db.execute(
                "CREATE TABLE IF NOT EXISTS schema_migrations ("
                "version INTEGER PRIMARY KEY, name TEXT NOT NULL, "
                "applied_at TEXT NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%fZ','now'))) ");
        const auto applied = appliedMigrations(db);
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
                db.execute(sql);
            }
            catch (const std::exception &error)
            {
                throw std::runtime_error("Migration " + name + " failed: " + error.what());
            }
            auto statement = db.prepare(
                "INSERT INTO schema_migrations(version, name) VALUES (?1, ?2)");
            statement.bindInt(1, version);
            statement.bindText(2, name);
            if (statement.step())
            {
                throw std::runtime_error("Cannot record migration " + name);
            }
            newlyApplied.push_back(name);
        }
        transaction.commit();
        for (const auto &name : alreadyApplied)
        {
            std::cout << "SKIP " << name << " (already applied)\n";
        }
        for (const auto &name : newlyApplied)
        {
            std::cout << "APPLY " << name << '\n';
        }
    }
}
}  // namespace campusfix
