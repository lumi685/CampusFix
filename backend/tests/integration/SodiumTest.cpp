#include <sodium.h>

#include <array>
#include <iostream>
#include <stdexcept>
#include <string>

int main()
{
    try
    {
        if (sodium_init() < 0)
        {
            throw std::runtime_error("libsodium initialization failed");
        }
        constexpr char password[] = "temporary test-only password";
        std::array<char, crypto_pwhash_STRBYTES> hash{};
        if (crypto_pwhash_str_alg(hash.data(), password, sizeof(password) - 1,
                                 crypto_pwhash_OPSLIMIT_INTERACTIVE,
                                 crypto_pwhash_MEMLIMIT_INTERACTIVE,
                                 crypto_pwhash_ALG_ARGON2ID13) != 0 ||
            crypto_pwhash_str_verify(hash.data(), password, sizeof(password) - 1) != 0 ||
            crypto_pwhash_str_verify(hash.data(), "wrong", 5) == 0)
        {
            throw std::runtime_error("Argon2id hash or verification failed");
        }
        std::array<unsigned char, 32> first{};
        std::array<unsigned char, 32> second{};
        randombytes_buf(first.data(), first.size());
        randombytes_buf(second.data(), second.size());
        if (sodium_memcmp(first.data(), second.data(), first.size()) == 0)
        {
            throw std::runtime_error("Random outputs unexpectedly match");
        }
        std::cout << "PASS libsodium initialized, Argon2id verified, random bytes generated\n";
        return 0;
    }
    catch (const std::exception &error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
