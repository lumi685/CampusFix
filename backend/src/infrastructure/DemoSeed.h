#pragma once

#include <filesystem>

namespace campusfix
{
// Maintenance-only operation. Requires schema migration 001; never runs at HTTP startup.
void applyDemoSeed(const std::filesystem::path &databasePath,
                   const std::filesystem::path &seedFile);
}
