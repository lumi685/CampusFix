#pragma once
#include "repositories/AuthRepository.h"
#include <filesystem>
#include <string>
#include <vector>

namespace campusfix {
struct LoginResult { std::string token, expiresAt; AuthUser user; };
struct MeResult { AuthUser user; std::vector<Grant> grants; std::int64_t workerId=0; };
class AuthService {
public:
    explicit AuthService(std::filesystem::path path):path_(std::move(path)){}
    LoginResult login(const std::string &username,const std::string &password);
    MeResult me(const std::string &token);
    bool logout(const std::string &token);
private: std::filesystem::path path_;
};
}
