#pragma once
#include <string>
#include <vector>
#include <cstdint>

namespace crypto {

struct EncryptedBlob {
    std::vector<uint8_t> data; // [magic|lens|salt|iv|cipher|tag]
};

EncryptedBlob encrypt(const std::string& secret,
                      const std::vector<uint8_t>& plaintext);

std::vector<uint8_t> decrypt(const std::string& secret,
                             const std::vector<uint8_t>& blob);

} // namespace crypto
