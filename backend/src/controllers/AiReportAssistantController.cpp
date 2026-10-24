#include "controllers/AiReportAssistantController.h"
#include "middleware/AuthGuard.h"
#include "infrastructure/AuthCrypto.h"
#include "infrastructure/ChatCompletionsProvider.h"
#include "services/AiDraftService.h"
#include "common/ResponseUtils.h"
#include <trantor/utils/ConcurrentTaskQueue.h>

namespace campusfix {
namespace {
struct AssistantRuntime {
    LlmConfig config=LlmConfig::environment();
    ChatCompletionsProvider provider{config};
    AiRequestLimits limits;
    // Destroy/join workers before destroying limits/provider. No event-loop blocking.
    trantor::ConcurrentTaskQueue workers{4,"campusfix-ai"};
};
void respond(HttpCallback &callback,const drogon::HttpResponsePtr &response,const std::string &rid) {
    response->addHeader("Cache-Control","no-store");
    LOG_INFO<<"AI request http="<<response->statusCode()<<" request_id="<<rid;
    callback(response);
}
}
void registerAiReportAssistantController(const std::filesystem::path &path) {
    auto runtime=std::make_shared<AssistantRuntime>();
    for(const bool draft:{false,true}) {
    drogon::app().registerHandler(draft?"/api/ai/report-assistant/draft":"/api/ai/report-assistant/chat",authenticated(path,
        [runtime,draft](const drogon::HttpRequestPtr &request,HttpCallback &&callback) {
            const auto identity=requestIdentity(request);const auto rid=identity->requestId;
            try {
                if(identity->role!="reporter"&&identity->role!="worker")
                    throw AiError(403,"ACTION_FORBIDDEN","Only reporting users may use the assistant");
                if(!request->parameters().empty())throw AiError(422,"AI_VALIDATION_ERROR","Query parameters are not accepted");
                const auto type=request->getHeader("Content-Type");
                if(type!="application/json"&&type!="application/json; charset=utf-8"&&type!="application/json;charset=utf-8")
                    throw AiError(415,"AI_UNSUPPORTED_MEDIA_TYPE","JSON content type is required");
                const auto messages=parseAiConversation(std::string(request->body()),draft?AiConversation::Draft:AiConversation::Chat);
                if(!runtime->config.valid())throw AiError(503,"AI_NOT_CONFIGURED","AI service is not configured");
                const auto lease=runtime->limits.acquire(tokenDigest(bearerToken(request)));
                // No Token, user identity, database path or conversation is logged or sent as metadata.
                runtime->workers.runTaskInQueue([runtimePtr=runtime.get(),messages,lease,callback=std::move(callback),rid,draft]() mutable {
                    drogon::HttpResponsePtr response;
                    try {
                        Json::Value data;
                        if(draft)data=AiDraftService(runtimePtr->provider).generate(messages);
                        else data["reply"]=runtimePtr->provider.reply(messages);
                        response=successResponse(data,rid);
                    }catch(const AiError &e){response=errorResponse(static_cast<drogon::HttpStatusCode>(e.status),e.code,e.what(),rid);}
                    catch(...){response=errorResponse(drogon::k502BadGateway,"AI_UPSTREAM_UNAVAILABLE","AI request failed",rid);}
                    respond(callback,response,rid);
                });
            }catch(const AiError &e){respond(callback,errorResponse(static_cast<drogon::HttpStatusCode>(e.status),e.code,e.what(),rid),rid);}
            catch(...){respond(callback,errorResponse(drogon::k500InternalServerError,"INTERNAL_ERROR","AI request failed",rid),rid);}
        }),{drogon::Post});
    }
}
}
