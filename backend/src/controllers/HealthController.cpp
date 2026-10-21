#include "HealthController.h"

#include "../common/ResponseUtils.h"

#include <drogon/drogon.h>

#include <exception>
#include <functional>
#include <string>

namespace campusfix
{
void registerHealthController()
{
    drogon::app().registerHandler(
        "/api/health",
        [](const drogon::HttpRequestPtr &,
           std::function<void(const drogon::HttpResponsePtr &)> &&callback) {
            const std::string requestId = newRequestId();
            try
            {
                Json::Value data(Json::objectValue);
                data["status"] = "ok";
                data["service"] = "campusfix-backend";
                auto response = successResponse(data, requestId);
                LOG_INFO << "request_id=" << requestId
                         << " method=GET path=/api/health status=200";
                callback(response);
            }
            catch (const std::exception &error)
            {
                LOG_ERROR << "request_id=" << requestId
                          << " method=GET path=/api/health status=500 error="
                          << error.what();
                callback(errorResponse(drogon::k500InternalServerError,
                                       "INTERNAL_ERROR",
                                       "Internal server error",
                                       requestId));
            }
        },
        {drogon::Get});
}
}  // namespace campusfix
