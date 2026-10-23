#include <drogon/drogon.h>

#include "controllers/HealthController.h"
#include "infrastructure/DatabaseMigration.h"
#include "infrastructure/DemoSeed.h"

#include <charconv>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>

namespace
{
constexpr std::uint16_t kDefaultDevelopmentPort = 8848;

std::uint16_t developmentPort()
{
#ifdef _MSC_VER
    char *allocatedPort = nullptr;
    std::size_t allocatedSize = 0;
    if (_dupenv_s(&allocatedPort, &allocatedSize, "CAMPUSFIX_PORT") != 0)
    {
        throw std::runtime_error("Unable to read CAMPUSFIX_PORT");
    }
    const std::unique_ptr<char, decltype(&std::free)> portStorage(allocatedPort, &std::free);
    const char *rawPort = portStorage.get();
#else
    const char *rawPort = std::getenv("CAMPUSFIX_PORT");
#endif
    if (rawPort == nullptr || *rawPort == '\0')
    {
        return kDefaultDevelopmentPort;
    }

    unsigned int parsedPort = 0;
    const std::string_view value{rawPort};
    const auto result = std::from_chars(value.data(), value.data() + value.size(), parsedPort);
    if (result.ec != std::errc{} || result.ptr != value.data() + value.size() ||
        parsedPort == 0 || parsedPort > 65535)
    {
        throw std::runtime_error("CAMPUSFIX_PORT must be an integer from 1 to 65535");
    }

    return static_cast<std::uint16_t>(parsedPort);
}
}  // namespace

int main(int argc, char *argv[])
{
    try
    {
        if (argc != 1)
        {
            if (argc != 6 || std::string_view(argv[2]) != "--db")
            {
                throw std::runtime_error(
                    "Usage: campusfix_backend --migrate --db <sqlite-file> --migrations-dir <directory> "
                    "OR --seed --db <sqlite-file> --seed-file <sql-file>");
            }
            if (std::string_view(argv[1]) == "--migrate" &&
                std::string_view(argv[4]) == "--migrations-dir")
            {
                campusfix::applyMigrations(std::filesystem::path(argv[3]),
                                           std::filesystem::path(argv[5]));
                std::cout << "Database migrations completed\n";
            }
            else if (std::string_view(argv[1]) == "--seed" &&
                     std::string_view(argv[4]) == "--seed-file")
            {
                campusfix::applyDemoSeed(std::filesystem::path(argv[3]),
                                         std::filesystem::path(argv[5]));
                std::cout << "Demo seed completed\n";
            }
            else
            {
                throw std::runtime_error("Unknown maintenance command or argument order");
            }
            return 0;
        }
        const auto port = developmentPort();
        LOG_INFO << "CampusFix backend starting on http://127.0.0.1:" << port;

        campusfix::registerHealthController();
        drogon::app().addListener("127.0.0.1", port);
        drogon::app().run();
        return 0;
    }
    catch (const std::exception &error)
    {
        LOG_ERROR << "CampusFix backend failed to start: " << error.what();
        return 1;
    }
}
