#pragma once
#include <json/json.h>
#include <chrono>
#include <cstdint>
#include <deque>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <vector>

namespace campusfix {
struct AiError : std::runtime_error {
    int status;
    std::string code;
    AiError(int statusValue, std::string codeValue, const char *message)
        : std::runtime_error(message), status(statusValue), code(std::move(codeValue)) {}
};
struct ChatMessage { std::string role, content; };
std::size_t aiTextLength(const std::string &text);
enum class AiConversation { Chat, Draft };
Json::Value parseAiJson(const std::string &body);
std::vector<ChatMessage> parseAiConversation(const std::string &body, AiConversation purpose);
std::vector<ChatMessage> parseAiMessages(const std::string &body);
struct LlmConfig {
    std::string baseUrl, apiKey, model;
    int timeoutMs = 30000;
    static LlmConfig environment();
    bool valid() const;
};
class ModelProvider {
public:
    virtual ~ModelProvider() = default;
    virtual std::string reply(const std::vector<ChatMessage> &messages) = 0;
    virtual std::string draftContent(const std::vector<ChatMessage> &messages) = 0;
};
// No database dependency. Quotas are process-local and include failed attempts.
class AiRequestLimits {
public:
    using Clock = std::chrono::steady_clock;
    std::shared_ptr<void> acquire(const std::string &session,
                                 Clock::time_point now = Clock::now());
private:
    struct Entry { bool active = false; std::deque<Clock::time_point> attempts; };
    std::mutex mutex_;
    std::map<std::string, Entry> entries_;
    std::deque<Clock::time_point> global_;
    int active_ = 0;
};
}
