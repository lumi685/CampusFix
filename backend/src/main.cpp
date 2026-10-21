#include <drogon/drogon.h>

#include <charconv>
#include <cstdint>
#include <cstdlib>
#include <stdexcept>
#include <string_view>

namespace
{
constexpr std::uint16_t kDefaultDevelopmentPort = 8848;

std::uint16_t developmentPort()
{
    const char *rawPort = std::getenv("CAMPUSFIX_PORT");
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

int main()
{
    try
    {
        const auto port = developmentPort();
        LOG_INFO << "CampusFix backend starting on http://127.0.0.1:" << port;

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
