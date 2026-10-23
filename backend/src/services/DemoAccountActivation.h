#pragma once
#include <filesystem>
#include <string>
namespace campusfix {
void setDemoPassword(const std::filesystem::path &db,const std::string &username,
                     const std::string &password);
void enableDemoAccount(const std::filesystem::path &db,const std::string &username);
std::string readSecretFromConsole();
}
