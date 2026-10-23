#pragma once

#include <filesystem>

namespace campusfix
{
// Explicit maintenance operation; normal HTTP startup never opens the database.
void applyMigrations(const std::filesystem::path &databasePath,
                     const std::filesystem::path &migrationsDirectory);
}
