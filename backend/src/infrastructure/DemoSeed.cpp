#include "infrastructure/DemoSeed.h"
#include "infrastructure/SqliteConnection.h"

#include <fstream>
#include <stdexcept>
#include <string>

namespace campusfix
{
void applyDemoSeed(const std::filesystem::path &databasePath,
                   const std::filesystem::path &seedFile)
{
    if (!std::filesystem::is_regular_file(seedFile))
    {
        throw std::runtime_error("Seed SQL file does not exist");
    }
    std::ifstream input(seedFile, std::ios::binary);
    if (!input)
    {
        throw std::runtime_error("Cannot read seed SQL file");
    }
    const std::string sql{std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
    if (sql.empty())
    {
        throw std::runtime_error("Seed SQL file is empty");
    }

    // No CREATE flag: --seed must never silently create an unmigrated database.
    SqliteConnection db(databasePath);
    try
    {
        if (db.scalarInt("SELECT COUNT(*) FROM schema_migrations "
                         "WHERE version=1 AND name='001_initial_schema.sql'") != 1)
        {
            throw std::runtime_error("Initial schema migration 001 is required before seed");
        }
    }
    catch (const std::exception &error)
    {
        throw std::runtime_error("Cannot seed before initial migration: " + std::string(error.what()));
    }

    SqliteTransaction transaction(db);
    try
    {
        db.execute(sql);
    }
    catch (const std::exception &error)
    {
        throw std::runtime_error("Demo seed failed; transaction rolled back: " +
                                 std::string(error.what()));
    }
    transaction.commit();
}
}  // namespace campusfix
