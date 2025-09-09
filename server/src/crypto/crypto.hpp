#ifndef LANCHAT_CRYPTO_CRYPTO_HPP
#define LANCHAT_CRYPTO_CRYPTO_HPP

#include <string>
#include <vector>
#include <cstdint>

namespace crypto {

/**
 * Содержит сериализованный blob: [magic(4) | lens(3) | salt | iv | ciphertext | tag]
 */
struct EncryptedBlob {
    std::vector<uint8_t> data;
};

/**
 * AES-256-GCM с PBKDF2(HMAC-SHA256) по строковому секрету.
 */
EncryptedBlob encrypt(const std::string& secret,
                      const std::vector<uint8_t>& plaintext);

std::vector<uint8_t> decrypt(const std::string& secret,
                             const std::vector<uint8_t>& blob);

} // namespace crypto

#endif // LANCHAT_CRYPTO_CRYPTO_HPP
