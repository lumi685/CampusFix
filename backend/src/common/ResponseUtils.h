#pragma once

#include <drogon/HttpResponse.h>
#include <drogon/utils/Utilities.h>

#include <string>

namespace campusfix
{
inline std::string newRequestId()
{
    return "req_" + drogon::utils::getUuid();
}

inline drogon::HttpResponsePtr successResponse(const Json::Value &data,
                                               const std::string &requestId)
{
    Json::Value body(Json::objectValue);
    body["code"] = 0;
    body["message"] = "success";
    body["data"] = data;
    body["request_id"] = requestId;
    return drogon::HttpResponse::newHttpJsonResponse(body);
}

inline drogon::HttpResponsePtr errorResponse(drogon::HttpStatusCode status,
                                             const std::string &code,
                                             const std::string &message,
                                             const std::string &requestId)
{
    Json::Value body(Json::objectValue);
    body["code"] = code;
    body["message"] = message;
    body["data"] = Json::nullValue;
    body["request_id"] = requestId;
    auto response = drogon::HttpResponse::newHttpJsonResponse(body);
    response->setStatusCode(status);
    return response;
}
}  // namespace campusfix
