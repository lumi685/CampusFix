#include "services/AiChatService.h"
#include <algorithm>
#include <charconv>
#include <cstdlib>
#include <sstream>

namespace campusfix {
namespace {
[[noreturn]] void invalid() { throw AiError(422,"AI_VALIDATION_ERROR","Invalid conversation"); }
std::string env(const char *name) {
    char *raw = nullptr; std::size_t size = 0;
    if (_dupenv_s(&raw, &size, name) != 0) return {};
    const std::unique_ptr<char, decltype(&std::free)> value(raw, &std::free);
    return raw ? raw : "";
}
}
std::size_t aiTextLength(const std::string &s) {
    std::size_t count = 0; bool content = false;
    for (std::size_t i = 0; i < s.size();) {
        const auto lead = static_cast<unsigned char>(s[i++]);
        std::uint32_t cp = lead; int extra = 0;
        if (lead >= 0xC2 && lead <= 0xDF) { cp=lead&31;extra=1; }
        else if (lead >= 0xE0 && lead <= 0xEF) { cp=lead&15;extra=2; }
        else if (lead >= 0xF0 && lead <= 0xF4) { cp=lead&7;extra=3; }
        else if (lead >= 0x80) invalid();
        if (i+extra > s.size()) invalid();
        for (int n=0;n<extra;++n) {
            const auto c=static_cast<unsigned char>(s[i++]);
            if ((c&0xC0)!=0x80) invalid();
            cp=(cp<<6)|(c&63);
        }
        if ((extra==1&&cp<0x80)||(extra==2&&cp<0x800)||(extra==3&&cp<0x10000)||
            cp>0x10FFFF||(cp>=0xD800&&cp<=0xDFFF)||cp==0||
            (cp<32&&cp!=9&&cp!=10&&cp!=13)||cp==127) invalid();
        const bool space=cp==32||cp==9||cp==10||cp==13||cp==0x85||cp==0xA0||
            cp==0x1680||(cp>=0x2000&&cp<=0x200A)||cp==0x2028||cp==0x2029||
            cp==0x202F||cp==0x205F||cp==0x3000||cp==0xFEFF||cp==0x200B;
        if (!space) content=true;
        ++count;
    }
    if (!content) invalid();
    return count;
}
Json::Value parseAiJson(const std::string &body) {
    Json::CharReaderBuilder builder;
    builder["collectComments"]=false;builder["allowComments"]=false;
    builder["rejectDupKeys"]=true;builder["failIfExtra"]=true;
    builder["allowTrailingCommas"]=false;
    builder["stackLimit"]=32;builder["allowSpecialFloats"]=false;
    Json::Value root;std::string error;
    const std::unique_ptr<Json::CharReader> reader(builder.newCharReader());
    if (!reader->parse(body.data(),body.data()+body.size(),&root,&error))
        throw AiError(400,"MALFORMED_REQUEST","Invalid JSON");
    return root;
}
std::vector<ChatMessage> parseAiConversation(const std::string &body, AiConversation purpose) {
    if (body.size()>65536) throw AiError(413,"AI_REQUEST_TOO_LARGE","Conversation is too large");
    const auto root=parseAiJson(body);
    if (!root.isObject()||root.size()!=1||!root.isMember("messages")||!root["messages"].isArray()) invalid();
    const auto &messages=root["messages"];
    if (messages.empty()) invalid();
    if (messages.size()>12) throw AiError(413,"AI_REQUEST_TOO_LARGE","Too many messages");
    std::vector<ChatMessage> result;std::size_t total=0;
    for (const auto &m:messages) {
        if (!m.isObject()||m.size()!=2||!m["role"].isString()||!m["content"].isString()) invalid();
        const auto role=m["role"].asString(),text=m["content"].asString();
        if (role!="user"&&role!="assistant") invalid();
        const auto length=aiTextLength(text);
        if (length>1500) throw AiError(413,"AI_REQUEST_TOO_LARGE","Message is too large");
        total+=length;
        if (total>9000) throw AiError(413,"AI_REQUEST_TOO_LARGE","Conversation is too large");
        result.push_back({role,text});
    }
    if (result.front().role!="user"||(purpose==AiConversation::Chat&&result.back().role!="user")) invalid();
    return result;
}
std::vector<ChatMessage> parseAiMessages(const std::string &body) {
    return parseAiConversation(body,AiConversation::Chat);
}
LlmConfig LlmConfig::environment() {
    LlmConfig c{env("CAMPUSFIX_LLM_BASE_URL"),env("CAMPUSFIX_LLM_API_KEY"),env("CAMPUSFIX_LLM_MODEL")};
    const auto timeout=env("CAMPUSFIX_LLM_TIMEOUT_MS");
    if (!timeout.empty()) {
        const auto p=std::from_chars(timeout.data(),timeout.data()+timeout.size(),c.timeoutMs);
        if (p.ec!=std::errc{}||p.ptr!=timeout.data()+timeout.size()) c.timeoutMs=0;
    }
    while (!c.baseUrl.empty()&&c.baseUrl.back()=='/') c.baseUrl.pop_back();
    return c;
}
bool LlmConfig::valid() const {
    // Non-HTTPS permitted ONLY on loopback for isolated test doubles/local development.
    const bool transport=baseUrl.rfind("https://",0)==0||baseUrl.rfind("http://127.0.0.1:",0)==0;
    if (!transport||baseUrl.size()>2048||baseUrl.find_first_of("\r\n\t ?#@\\")!=std::string::npos||
        apiKey.empty()||apiKey.size()>1024||model.empty()||model.size()>128||timeoutMs<100||timeoutMs>45000) return false;
    for (const unsigned char c:apiKey) if (c<33||c>126) return false;
    for (const unsigned char c:model) if (!((c>='a'&&c<='z')||(c>='A'&&c<='Z')||
        (c>='0'&&c<='9')||c=='-'||c=='_'||c=='.'||c=='/')) return false;
    return true;
}
std::shared_ptr<void> AiRequestLimits::acquire(const std::string &session,Clock::time_point now) {
    std::lock_guard<std::mutex> lock(mutex_);
    const auto prune=[now](auto &q) {while(!q.empty()&&now-q.front()>=std::chrono::hours(1))q.pop_front();};
    prune(global_);
    for(auto it=entries_.begin();it!=entries_.end();) {
        prune(it->second.attempts);
        if(!it->second.active&&it->second.attempts.empty())it=entries_.erase(it);else ++it;
    }
    if(entries_.find(session)==entries_.end()&&entries_.size()>=1024)
        throw AiError(429,"AI_RATE_LIMITED","AI request limit reached");
    auto &entry=entries_[session];
    if(entry.active)throw AiError(429,"AI_REQUEST_IN_PROGRESS","A request is already running");
    if(active_>=4)throw AiError(429,"AI_BUSY","AI service is busy");
    const auto recent=std::count_if(entry.attempts.begin(),entry.attempts.end(),[now](const auto &t){return now-t<std::chrono::minutes(1);});
    if(recent>=10||entry.attempts.size()>=60||global_.size()>=200)
        throw AiError(429,"AI_RATE_LIMITED","AI request limit reached");
    entry.active=true;entry.attempts.push_back(now);global_.push_back(now);++active_;
    return std::shared_ptr<void>(new int(0),[this,session](void *p){
        delete static_cast<int *>(p);std::lock_guard<std::mutex> guard(mutex_);
        entries_.at(session).active=false;--active_;
    });
}
}
