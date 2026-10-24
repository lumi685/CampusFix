#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <winhttp.h>
#include "infrastructure/ChatCompletionsProvider.h"
#include "AiSystemPrompt.h"
#include <array>
#include <algorithm>

namespace campusfix {
namespace {
struct InternetHandle {
    HINTERNET value;
    explicit InternetHandle(HINTERNET h):value(h) {
        if(!h)throw AiError(502,"AI_UPSTREAM_UNAVAILABLE","AI provider connection failed");
    }
    ~InternetHandle(){WinHttpCloseHandle(value);}
    InternetHandle(const InternetHandle &)=delete;
    InternetHandle &operator=(const InternetHandle &)=delete;
};
std::wstring wide(const std::string &s) {
    const int size=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,s.data(),static_cast<int>(s.size()),nullptr,0);
    if(size<=0)throw AiError(503,"AI_NOT_CONFIGURED","AI configuration is invalid");
    std::wstring w(static_cast<std::size_t>(size),L'\0');
    MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,s.data(),static_cast<int>(s.size()),w.data(),size);
    return w;
}
void check(BOOL ok) {
    if(ok)return;
    if(GetLastError()==ERROR_WINHTTP_TIMEOUT)throw AiError(504,"AI_UPSTREAM_TIMEOUT","AI provider timed out");
    throw AiError(502,"AI_UPSTREAM_UNAVAILABLE","AI provider connection failed");
}
[[noreturn]] void invalidReply(bool structured) {
    throw AiError(502,structured?"AI_DRAFT_INVALID_RESPONSE":"AI_INVALID_RESPONSE","AI provider returned an invalid reply");
}
bool containsSecret(const Json::Value &value,const std::string &secret) {
    if(value.isString())return value.asString().find(secret)!=std::string::npos;
    if(value.isArray()||value.isObject())for(const auto &child:value)if(containsSecret(child,secret))return true;
    return false;
}
}
std::string ChatCompletionsProvider::reply(const std::vector<ChatMessage> &messages) {
    return complete(messages,false);
}
std::string ChatCompletionsProvider::draftContent(const std::vector<ChatMessage> &messages) {
    return complete(messages,true);
}
std::string ChatCompletionsProvider::complete(const std::vector<ChatMessage> &messages,bool structured) {
    if(!config_.valid())throw AiError(503,"AI_NOT_CONFIGURED","AI service is not configured");
    const auto url=wide(config_.baseUrl+"/chat/completions");
    URL_COMPONENTS components{};components.dwStructSize=sizeof(components);
    components.dwHostNameLength=static_cast<DWORD>(-1);components.dwUrlPathLength=static_cast<DWORD>(-1);
    if(!WinHttpCrackUrl(url.c_str(),static_cast<DWORD>(url.size()),0,&components))
        throw AiError(503,"AI_NOT_CONFIGURED","AI configuration is invalid");
    const std::wstring host(components.lpszHostName,components.dwHostNameLength);
    const bool secure=components.nScheme==INTERNET_SCHEME_HTTPS;
    if(!secure&&(host!=L"127.0.0.1"||components.nScheme!=INTERNET_SCHEME_HTTP))
        throw AiError(503,"AI_NOT_CONFIGURED","AI configuration is invalid");
    const std::wstring path(components.lpszUrlPath,components.dwUrlPathLength);
    Json::Value payload(Json::objectValue);payload["model"]=config_.model;
    payload["stream"]=false;payload["max_tokens"]=structured?4096:512;
    if(structured)payload["response_format"]["type"]="json_object";
    // Explicitly disable thinking for this short, non-tool text assistant (DeepSeek protocol).
    payload["thinking"]["type"]="disabled";
    payload["messages"]=Json::arrayValue;
    Json::Value system;system["role"]="system";system["content"]=structured?aiDraftSystemPrompt:aiSystemPrompt;
    payload["messages"].append(system);
    for(const auto &m:messages){Json::Value item;item["role"]=m.role;item["content"]=m.content;payload["messages"].append(item);}
    if(structured) {
        Json::Value instruction;instruction["role"]="user";
        instruction["content"]="请仅整理上述当前保留对话中的实际故障信息，按照系统约定返回六字段 JSON 建议对象；未知信息用 null，不创建工单。";
        payload["messages"].append(instruction);
    }
    Json::StreamWriterBuilder writer;writer["indentation"]="";
    const auto body=Json::writeString(writer,payload);
    InternetHandle session(WinHttpOpen(L"CampusFix/1.2",secure?WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY:WINHTTP_ACCESS_TYPE_NO_PROXY,
        WINHTTP_NO_PROXY_NAME,WINHTTP_NO_PROXY_BYPASS,0));
    check(WinHttpSetTimeouts(session.value,config_.timeoutMs,config_.timeoutMs,config_.timeoutMs,config_.timeoutMs));
    InternetHandle connection(WinHttpConnect(session.value,host.c_str(),components.nPort,0));
    InternetHandle request(WinHttpOpenRequest(connection.value,L"POST",path.c_str(),nullptr,
        WINHTTP_NO_REFERER,WINHTTP_DEFAULT_ACCEPT_TYPES,secure?WINHTTP_FLAG_SECURE:0));
    // Never follow redirects with a model key; never use ambient Windows credentials/cookies.
    DWORD disabled=WINHTTP_DISABLE_REDIRECTS|WINHTTP_DISABLE_AUTHENTICATION|WINHTTP_DISABLE_COOKIES;
    check(WinHttpSetOption(request.value,WINHTTP_OPTION_DISABLE_FEATURE,&disabled,sizeof(disabled)));
    DWORD autoLogon=WINHTTP_AUTOLOGON_SECURITY_LEVEL_HIGH;
    check(WinHttpSetOption(request.value,WINHTTP_OPTION_AUTOLOGON_POLICY,&autoLogon,sizeof(autoLogon)));
    DWORD headerLimit=16384;
    check(WinHttpSetOption(request.value,WINHTTP_OPTION_MAX_RESPONSE_HEADER_SIZE,&headerLimit,sizeof(headerLimit)));
    const auto deadline=std::chrono::steady_clock::now()+std::chrono::milliseconds(config_.timeoutMs);
    const auto remaining=[&] {
        const auto left=std::chrono::duration_cast<std::chrono::milliseconds>(deadline-std::chrono::steady_clock::now()).count();
        if(left<=0)throw AiError(504,"AI_UPSTREAM_TIMEOUT","AI provider timed out");
        const auto ms=static_cast<int>(left);
        check(WinHttpSetTimeouts(request.value,ms,ms,ms,ms));
    };
    const auto headers=wide("Content-Type: application/json\r\nAccept: application/json\r\nAccept-Encoding: identity\r\nAuthorization: Bearer "+config_.apiKey+"\r\n");
    remaining();
    check(WinHttpSendRequest(request.value,headers.c_str(),static_cast<DWORD>(-1),
        const_cast<char *>(body.data()),static_cast<DWORD>(body.size()),static_cast<DWORD>(body.size()),0));
    remaining();check(WinHttpReceiveResponse(request.value,nullptr));
    DWORD status=0,size=sizeof(status);
    check(WinHttpQueryHeaders(request.value,WINHTTP_QUERY_STATUS_CODE|WINHTTP_QUERY_FLAG_NUMBER,
        WINHTTP_HEADER_NAME_BY_INDEX,&status,&size,WINHTTP_NO_HEADER_INDEX));
    // Error bodies are deliberately not read, logged or returned.
    if(status==401||status==403)throw AiError(502,"AI_PROVIDER_AUTH_FAILED","AI provider credentials were rejected");
    if(status==429)throw AiError(503,"AI_PROVIDER_RATE_LIMITED","AI provider is busy; retry later");
    if(status!=200)throw AiError(502,"AI_UPSTREAM_UNAVAILABLE","AI provider request failed");
    std::string received;std::array<char,8192> buffer{};
    for(;;) {
        remaining();DWORD read=0;check(WinHttpReadData(request.value,buffer.data(),static_cast<DWORD>(buffer.size()),&read));
        if(!read)break;
        if(received.size()+read>65536)throw AiError(502,"AI_RESPONSE_TOO_LARGE","AI provider reply is too large");
        received.append(buffer.data(),read);
    }
    Json::CharReaderBuilder parser;parser["collectComments"]=false;parser["allowComments"]=false;
    parser["failIfExtra"]=true;parser["rejectDupKeys"]=true;parser["stackLimit"]=32;
    parser["allowTrailingCommas"]=false;
    Json::Value root;std::string error;const std::unique_ptr<Json::CharReader> reader(parser.newCharReader());
    if(!reader->parse(received.data(),received.data()+received.size(),&root,&error)||!root.isObject()||
        !root["choices"].isArray()||root["choices"].size()!=1)invalidReply(structured);
    const auto &choice=root["choices"][0];
    if(!choice.isObject()||!choice["finish_reason"].isString()||choice["finish_reason"].asString()!="stop"||
        !choice["message"].isObject()||!choice["message"]["role"].isString()||choice["message"]["role"].asString()!="assistant"||
        !choice["message"]["content"].isString()||choice["message"].isMember("tool_calls"))invalidReply(structured);
    const auto reply=choice["message"]["content"].asString();
    try {const auto length=aiTextLength(reply);if(!structured&&length>1500)invalidReply(false);}catch(const AiError &){invalidReply(structured);}
    if(reply.find(config_.apiKey)!=std::string::npos)invalidReply(structured);
    if(structured) {
        // Inner JSON escapes are decoded again before reaching the browser.
        // Do not let that second decoding bypass the credential output guard.
        try {if(containsSecret(parseAiJson(reply),config_.apiKey))invalidReply(true);}
        catch(...){invalidReply(true);}
    }
    return reply;
}
}
