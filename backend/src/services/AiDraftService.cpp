#include "services/AiDraftService.h"
#include <array>

namespace campusfix {
namespace {
[[noreturn]] void invalidDraft() {
    throw AiError(502,"AI_DRAFT_INVALID_RESPONSE","AI provider returned an invalid suggestion");
}
}
Json::Value validateAiDraft(const std::string &content) {
    try {
        if(content.size()>65536)invalidDraft();
        const auto root=parseAiJson(content);
        if(!root.isObject()||root.size()!=6)invalidDraft();
        const std::array<std::pair<const char *,std::size_t>,5> fields{{
            {"title",120},{"description",2000},{"category_hint",120},{"location_hint",200},{"safety_note",300}}};
        Json::Value safe(Json::objectValue);
        for(const auto &field:fields) {
            if(!root.isMember(field.first))invalidDraft();
            const auto &value=root[field.first];
            if(!value.isNull()&&(!value.isString()||aiTextLength(value.asString())>field.second))invalidDraft();
            safe[field.first]=value;
        }
        if(!root.isMember("missing_information")||!root["missing_information"].isArray()||root["missing_information"].size()>5)invalidDraft();
        safe["missing_information"]=Json::arrayValue;
        for(const auto &item:root["missing_information"]) {
            if(!item.isString()||aiTextLength(item.asString())>120)invalidDraft();
            safe["missing_information"].append(item);
        }
        return safe;
    }catch(...){invalidDraft();}
}
Json::Value AiDraftService::generate(const std::vector<ChatMessage> &messages) {
    const auto draft=validateAiDraft(provider_.draftContent(messages));
    Json::Value result(Json::objectValue);result["draft"]=draft;
    result["requires_review"]=true; // The model cannot opt out of human review.
    return result;
}
}
