#pragma once
#include "common/ResponseUtils.h"
#include "domain/AuthorizationPolicy.h"
namespace campusfix {
inline drogon::HttpResponsePtr accessErrorResponse(const AccessDecision &decision,const std::string &requestId) {
    auto response=errorResponse(static_cast<drogon::HttpStatusCode>(decision.status),decision.code,
        decision.status==404?"Resource not found":"Operation not allowed",requestId);
    response->addHeader("Cache-Control","no-store");return response;
}
}
