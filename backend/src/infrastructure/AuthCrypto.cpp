#include "infrastructure/AuthCrypto.h"
#include <sodium.h>
#include <array>
#include <stdexcept>
#include <string>

namespace campusfix {
void initializeCrypto() { if (sodium_init() < 0) throw std::runtime_error("Crypto initialization failed"); }
std::string hashPassword(std::string_view password) {
    std::array<char, crypto_pwhash_STRBYTES> output{};
    if (crypto_pwhash_str_alg(output.data(), password.data(), password.size(),
        crypto_pwhash_OPSLIMIT_INTERACTIVE, crypto_pwhash_MEMLIMIT_INTERACTIVE,
        crypto_pwhash_ALG_ARGON2ID13) != 0) throw std::runtime_error("Password hash failed");
    return output.data();
}
bool verifyPassword(std::string_view password, const std::string &hash) {
    if (hash.rfind("$argon2id$", 0) != 0) return false;
    return crypto_pwhash_str_verify(hash.c_str(), password.data(), password.size()) == 0;
}
std::string newToken() {
    std::array<unsigned char, 32> bytes{};
    randombytes_buf(bytes.data(), bytes.size());
    std::array<char, sodium_base64_ENCODED_LEN(32, sodium_base64_VARIANT_URLSAFE_NO_PADDING)> encoded{};
    sodium_bin2base64(encoded.data(), encoded.size(), bytes.data(), bytes.size(),
                      sodium_base64_VARIANT_URLSAFE_NO_PADDING);
    sodium_memzero(bytes.data(), bytes.size());
    return std::string("cf_live_") + encoded.data();
}
std::string tokenDigest(std::string_view token) {
    std::array<unsigned char, crypto_hash_sha256_BYTES> digest{};
    crypto_hash_sha256(digest.data(), reinterpret_cast<const unsigned char *>(token.data()), token.size());
    std::array<char, crypto_hash_sha256_BYTES * 2 + 1> hex{};
    sodium_bin2hex(hex.data(), hex.size(), digest.data(), digest.size());
    return hex.data();
}
std::string newOpaqueId() {
    std::array<unsigned char, 16> bytes{};
    randombytes_buf(bytes.data(), bytes.size());
    std::array<char, 33> hex{};
    sodium_bin2hex(hex.data(), hex.size(), bytes.data(), bytes.size());
    return hex.data();
}
}
