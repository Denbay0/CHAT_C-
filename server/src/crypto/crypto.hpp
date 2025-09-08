#ifndef LANCHAT_CRYPTO_CRYPTO_HPP
#define LANCHAT_CRYPTO_CRYPTO_HPP

#include <vector>
#include <cstdint>
#include "storage/storage.hpp"

namespace lanchat {

GcmBlob aes_gcm_encrypt_win(const std::vector<uint8_t>& key,
                            const std::vector<uint8_t>& plain,
                            const std::vector<uint8_t>& aad = {});

std::vector<uint8_t> aes_gcm_decrypt_win(const std::vector<uint8_t>& key,
                                         const GcmBlob& blob,
                                         const std::vector<uint8_t>& aad = {});

}

#endif
