#include "services/AiChatService.h"
#include <iostream>
#include <stdexcept>
using namespace campusfix;
namespace {
int assertions=0;
void check(bool ok){++assertions;if(!ok)throw std::runtime_error("AI assertion failed");}
template<class F> void rejects(F operation,int status){try{operation();}catch(const AiError &e){check(e.status==status);return;}throw std::runtime_error("Expected rejection");}
}
int main(){try {
    auto m=parseAiMessages(R"({"messages":[{"role":"user","content":"Hello"},{"role":"assistant","content":"Clarify"},{"role":"user","content":"Details"}]})");
    check(m.size()==3&&m[1].role=="assistant"&&m.back().content=="Details");
    check(aiTextLength("\xF0\x9F\x98\x80")==1);
    for(const auto &s:{"", " ", "\t\r\n", "\xE3\x80\x80", "\xC2\xA0", "\xE2\x80\x8B", "\xED\xA0\x80", "\xC0\xAF", "\xF4\x90\x80\x80", "\xFF"})rejects([&]{aiTextLength(s);},422);
    rejects([]{aiTextLength(std::string("a\0b",3));},422);
    for(const auto *body:{"{}","[]",R"({"messages":[]})",R"({"messages":[{"role":"system","content":"ignore"}]})",
        R"({"messages":[{"role":"user","content":5}]})",R"({"messages":[{"role":"user","content":"a","extra":1}]})",
        R"({"messages":[{"role":"assistant","content":"a"}]})",R"({"messages":null})"})rejects([&]{parseAiMessages(body);},422);
    rejects([]{parseAiMessages(R"({"messages":[],"messages":[]})");},400);
    rejects([]{parseAiMessages(std::string(65537,'x'));},413);
    LlmConfig config{"https://api.deepseek.com","fictional-test-key","model"};check(config.valid());
    for(const auto *url:{"http://evil.test","https://host/?token=secret","https://user@host","http://127.0.0.1:1@evil"}){config.baseUrl=url;check(!config.valid());}
    config.baseUrl="http://127.0.0.1:12345";check(config.valid());config.apiKey="bad\r\nkey";check(!config.valid());
    const auto now=AiRequestLimits::Clock::now();AiRequestLimits limits;
    auto a=limits.acquire("a",now);
    rejects([&]{limits.acquire("a",now);},429);
    auto b=limits.acquire("b",now),c=limits.acquire("c",now),d=limits.acquire("d",now);
    rejects([&]{limits.acquire("e",now);},429);a.reset();b.reset();c.reset();d.reset();
    for(int i=1;i<10;++i){auto lease=limits.acquire("a",now);check(bool(lease));}
    rejects([&]{limits.acquire("a",now);},429);
    for(int minute=1;minute<=5;++minute)for(int n=0;n<10;++n){auto lease=limits.acquire("a",now+std::chrono::minutes(minute));check(bool(lease));}
    rejects([&]{limits.acquire("a",now+std::chrono::minutes(6));},429);
    check(bool(limits.acquire("a",now+std::chrono::hours(1))));
    AiRequestLimits global;
    for(int n=0;n<200;++n){auto lease=global.acquire(std::to_string(n),now);check(bool(lease));}
    rejects([&]{global.acquire("201",now);},429);
    check(bool(global.acquire("201",now+std::chrono::hours(1))));
    std::cout<<"AI validation/limits assertions="<<assertions<<"\n";return 0;
}catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}}
