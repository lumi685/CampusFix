#pragma once
#include "services/AiChatService.h"

namespace campusfix {
// An untrusted text suggestion, never a WorkOrder or a database entity.
Json::Value validateAiDraft(const std::string &content);
class AiDraftService {
public:
    explicit AiDraftService(ModelProvider &provider):provider_(provider){}
    Json::Value generate(const std::vector<ChatMessage> &messages);
private:
    ModelProvider &provider_;
};
}
