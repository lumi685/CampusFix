#include <drogon/drogon.h>

#include "controllers/HealthController.h"
#include "controllers/AuthController.h"
#include "controllers/WorkOrderController.h"
#include "controllers/WorkerController.h"
#include "controllers/DispatchController.h"
#include "controllers/AiReportAssistantController.h"
#include "infrastructure/DatabaseMigration.h"
#include "infrastructure/DemoSeed.h"
#include "infrastructure/AuthCrypto.h"
#include "services/DemoAccountActivation.h"
#include <sodium.h>

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
        campusfix::initializeCrypto();
        if (argc != 1)
        {
            if (argc == 6 && std::string_view(argv[2]) == "--db" &&
                std::string_view(argv[4]) == "--username")
            {
                if (std::string_view(argv[1]) == "--set-demo-password")
                {
                    std::string secret = campusfix::readSecretFromConsole();
                    try { campusfix::setDemoPassword(argv[3], argv[5], secret); }
                    catch (...) { sodium_memzero(secret.data(), secret.size()); throw; }
                    sodium_memzero(secret.data(), secret.size());
                    std::cout << "Demo password set; account remains disabled\n";
                    return 0;
                }
                if (std::string_view(argv[1]) == "--enable-demo")
                {
                    campusfix::enableDemoAccount(argv[3], argv[5]);
                    std::cout << "Named demo account enabled\n";
                    return 0;
                }
            }
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
        char *allocatedDb=nullptr;
        std::size_t allocatedDbSize=0;
        if (_dupenv_s(&allocatedDb,&allocatedDbSize,"CAMPUSFIX_DB_PATH") != 0)
            throw std::runtime_error("Unable to read CAMPUSFIX_DB_PATH");
        const std::unique_ptr<char, decltype(&std::free)> dbStorage(allocatedDb,&std::free);
        campusfix::registerAuthController(allocatedDb && *allocatedDb ?
            std::filesystem::path(allocatedDb) : std::filesystem::path{});
        campusfix::registerWorkOrderController(allocatedDb && *allocatedDb ?
            std::filesystem::path(allocatedDb) : std::filesystem::path{});
        campusfix::registerWorkerController(allocatedDb && *allocatedDb ?
            std::filesystem::path(allocatedDb) : std::filesystem::path{});
        campusfix::registerDispatchController(allocatedDb && *allocatedDb ?
            std::filesystem::path(allocatedDb) : std::filesystem::path{});
        campusfix::registerAiReportAssistantController(allocatedDb && *allocatedDb ?
            std::filesystem::path(allocatedDb) : std::filesystem::path{});
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
