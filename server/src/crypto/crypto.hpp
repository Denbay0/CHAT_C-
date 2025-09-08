#pragma once
#include <vector>
#include <string>
#include <cstdint>
#include <optional>

namespace lanchat {

struct GcmBlob {
  std::vector<uint8_t> iv;   // 12 bytes
  std::vector<uint8_t> tag;  // 16 bytes
  std::vector<uint8_t> ct;   // ciphertext
};

// encrypt/decrypt. Если платформа не Windows или ключ пустой — бросают исключение (encrypt/decrypt) или не используются.
GcmBlob aes_gcm_encrypt_win(const std::vector<uint8_t>& key32,
                            const std::vector<uint8_t>& plain,
                            const std::vector<uint8_t>& aad = {});
std::vector<uint8_t> aes_gcm_decrypt_win(const std::vector<uint8_t>& key32,
                                         const GcmBlob& blob,
                                         const std::vector<uint8_t>& aad = {});

} // namespace lanchat
