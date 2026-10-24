#include "services/AiDraftService.h"
#include <iostream>
using namespace campusfix;
namespace {
int assertions=0;
void check(bool value){++assertions;if(!value)throw std::runtime_error("Draft assertion failed");}
template<class F>void rejects(F f,int status,const char *code=nullptr){try{f();}catch(const AiError &e){check(e.status==status);if(code)check(e.code==code);return;}throw std::runtime_error("Expected draft rejection after assertion "+std::to_string(assertions));}
std::string json(const Json::Value &v){Json::StreamWriterBuilder w;w["indentation"]="";return Json::writeString(w,v);}
Json::Value valid(){Json::Value d;d["title"]="Fictional drain leak";d["description"]="Fictional reported symptoms";d["category_hint"]=Json::nullValue;d["location_hint"]="Fictional room; exact location unknown";d["safety_note"]=Json::nullValue;d["missing_information"]=Json::arrayValue;d["missing_information"].append("Building and room");return d;}
struct Provider : ModelProvider {
    std::string output=json(valid());int calls=0;
    std::string reply(const std::vector<ChatMessage> &) override {throw std::runtime_error("Chat mode must not be used");}
    std::string draftContent(const std::vector<ChatMessage> &m) override {check(!m.empty());++calls;return output;}
};
}
int main(){try {
    const std::string history=R"({"messages":[{"role":"user","content":"lamp failure"},{"role":"assistant","content":"clarify"}]})";
    const auto messages=parseAiConversation(history,AiConversation::Draft);check(messages.size()==2&&messages.back().role=="assistant");
    rejects([&]{parseAiMessages(history);},422);check(parseAiMessages(R"({"messages":[{"role":"user","content":"lamp failure"}]})").size()==1);
    for(const auto *s:{"{}","[]",R"({"messages":[]})",R"({"messages":[{"role":"assistant","content":"x"}]})",R"({"messages":[{"role":"system","content":"x"}]})",R"({"messages":[{"role":"developer","content":"x"}]})",R"({"messages":[{"role":"tool","content":"x"}]})",R"({"messages":[{"role":"user","content":" "}]})",R"({"messages":[{"role":"user","content":1}]})",R"({"messages":[{"role":"user","content":"x","id":1}]})"})rejects([&]{parseAiConversation(s,AiConversation::Draft);},422);
    rejects([]{parseAiConversation(R"({"messages":[],"messages":[]})",AiConversation::Draft);},400);
    Provider provider;const auto result=AiDraftService(provider).generate(messages);
    check(provider.calls==1&&result["requires_review"].asBool());check(result["draft"]==valid());
    const std::pair<const char *,int> fields[]={{"title",120},{"description",2000},{"category_hint",120},{"location_hint",200},{"safety_note",300}};
    for(const auto &field:fields){
        auto d=valid();d[field.first]=Json::nullValue;check(validateAiDraft(json(d))[field.first].isNull());
        d[field.first]=std::string(field.second,'x');check(validateAiDraft(json(d))[field.first].asString().size()==static_cast<std::size_t>(field.second));
        d[field.first]=std::string(field.second+1,'x');rejects([&]{validateAiDraft(json(d));},502,"AI_DRAFT_INVALID_RESPONSE");
        for(const auto &bad:{Json::Value(""),Json::Value("　"),Json::Value(1),Json::Value(true),Json::Value(Json::arrayValue),Json::Value(std::string("a\0b",3))}){d[field.first]=bad;rejects([&]{validateAiDraft(json(d));},502,"AI_DRAFT_INVALID_RESPONSE");}
        d=valid();d.removeMember(field.first);rejects([&]{validateAiDraft(json(d));},502,"AI_DRAFT_INVALID_RESPONSE");
    }
    for(const auto *extra:{"category_id","location_id","order_id","requires_review","sql","tool_calls"}){auto d=valid();d[extra]=1;rejects([&]{validateAiDraft(json(d));},502,"AI_DRAFT_INVALID_RESPONSE");}
    for(const auto &bad:{"", " ", "{}", "[]", "null", "```json\n{}\n```", "{", "{\"title\":null,\"title\":null}", "{} trailing"})rejects([&]{validateAiDraft(bad);},502,"AI_DRAFT_INVALID_RESPONSE");
    for(int size:{0,5,6}){auto d=valid();d["missing_information"]=Json::arrayValue;for(int i=0;i<size;++i)d["missing_information"].append("known missing info");if(size>5)rejects([&]{validateAiDraft(json(d));},502);else check(validateAiDraft(json(d))["missing_information"].size()==static_cast<Json::ArrayIndex>(size));}
    for(const auto &bad:{Json::Value(" "),Json::Value(1),Json::Value(Json::nullValue),Json::Value(std::string(121,'x'))}){auto d=valid();d["missing_information"][0]=bad;rejects([&]{validateAiDraft(json(d));},502,"AI_DRAFT_INVALID_RESPONSE");}
    auto d=valid();d["title"]="\xF0\x9F\x98\x80";check(validateAiDraft(json(d))["title"]==d["title"]);
    // JsonCpp's writer sanitizes invalid UTF-8. Prove that behavior, then inject
    // raw invalid bytes AFTER serialization to exercise the real validation boundary.
    d=valid();d["title"]=std::string("\xFF");check(validateAiDraft(json(d))["title"].asString()=="\xEF\xBF\xBD");
    auto raw=json(valid());const auto position=raw.find("Fictional drain leak");check(position!=std::string::npos);
    raw.replace(position,std::string("Fictional drain leak").size(),std::string("\xFF"));rejects([&]{validateAiDraft(raw);},502,"AI_DRAFT_INVALID_RESPONSE");
    provider.output="bad-json";rejects([&]{AiDraftService(provider).generate(messages);},502,"AI_DRAFT_INVALID_RESPONSE");check(provider.calls==2);
    std::cout<<"AI Draft Service assertions="<<assertions<<'\n';return 0;
}catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}}
