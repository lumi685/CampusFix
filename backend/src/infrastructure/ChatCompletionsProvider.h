#pragma once
#include "services/AiChatService.h"
namespace campusfix {
// DeepSeek's documented non-streaming Chat Completions wire protocol.
// Transport can be replaced without changing the controller or browser contract.
class ChatCompletionsProvider final : public ModelProvider {
public:
    explicit ChatCompletionsProvider(LlmConfig config):config_(std::move(config)){}
    std::string reply(const std::vector<ChatMessage> &messages) override;
private:
    LlmConfig config_;
};
}
