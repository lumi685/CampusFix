#pragma once
#include <string>
#include <string_view>

namespace campusfix {
void initializeCrypto();
std::string hashPassword(std::string_view password);
bool verifyPassword(std::string_view password, const std::string &hash);
std::string newToken();
std::string tokenDigest(std::string_view token);
std::string newOpaqueId();
}
