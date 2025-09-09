#include "crypto.hpp"

#include <windows.h>
#include <bcrypt.h>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

#pragma comment(lib, "bcrypt.lib")

namespace crypto {

// --- параметры схемы ---
static constexpr ULONG AES_KEYLEN_BYTES = 32; // AES-256
static constexpr ULONG GCM_IV_LEN       = 12; // 96-bit nonce (рекомендовано для GCM)
static constexpr ULONG GCM_TAG_LEN      = 16; // 128-bit tag
static constexpr ULONG SALT_LEN         = 16; // 128-bit salt
static constexpr ULONG PBKDF2_ITERS     = 150000; // итерации PBKDF2

// RAII для закрытия провайдера
struct AlgHandle {
    BCRYPT_ALG_HANDLE h = nullptr;
    ~AlgHandle() { if (h) BCryptCloseAlgorithmProvider(h, 0); }
};

static void check(NTSTATUS s, const char* what) {
    if (!BCRYPT_SUCCESS(s)) {
        // если нужно детальней — раскомментируй и увидишь код NTSTATUS
        // char buf[64]; sprintf_s(buf, " (NTSTATUS=0x%08X)", (unsigned)s);
        // throw std::runtime_error(std::string(what) + " failed" + buf);
        throw std::runtime_error(std::string(what) + " failed");
    }
}

static std::vector<uint8_t> gen_random(ULONG len) {
    std::vector<uint8_t> buf(len);
    check(BCryptGenRandom(nullptr, buf.data(), len, BCRYPT_USE_SYSTEM_PREFERRED_RNG),
          "BCryptGenRandom");
    return buf;
}

// PBKDF2-HMAC-SHA256(secret, salt, iters) -> key(32)
// ВАЖНО: SHA256 надо открыть с флагом BCRYPT_ALG_HANDLE_HMAC_FLAG.
static std::vector<uint8_t> derive_key_pbkdf2(const std::string& secret,
                                              const std::vector<uint8_t>& salt,
                                              ULONG iters) {
    AlgHandle h;
    check(BCryptOpenAlgorithmProvider(&h.h,
                                      BCRYPT_SHA256_ALGORITHM,
                                      nullptr,
                                      BCRYPT_ALG_HANDLE_HMAC_FLAG),
          "Open SHA256(HMAC)");
    std::vector<uint8_t> key(AES_KEYLEN_BYTES);
    check(BCryptDeriveKeyPBKDF2(
              h.h,
              reinterpret_cast<PUCHAR>(const_cast<char*>(secret.data())),
              static_cast<ULONG>(secret.size()),
              const_cast<PUCHAR>(salt.data()),
              static_cast<ULONG>(salt.size()),
              iters,
              key.data(),
              static_cast<ULONG>(key.size()),
              0),
          "BCryptDeriveKeyPBKDF2");
    return key;
}

// Внутреннее: шифрование AES-GCM с солью/KDF — возвращает сериализованный blob
static std::vector<uint8_t> encrypt_gcm_with_salt_blob(const std::string& secret,
                                                       const std::vector<uint8_t>& plaintext,
                                                       const std::vector<uint8_t>& aad = {}) {
    // 1) соль и IV
    auto salt = gen_random(SALT_LEN);
    auto iv   = gen_random(GCM_IV_LEN);

    // 2) derive key
    auto key  = derive_key_pbkdf2(secret, salt, PBKDF2_ITERS);

    // 3) AES-GCM провайдер
    AlgHandle a;
    check(BCryptOpenAlgorithmProvider(&a.h, BCRYPT_AES_ALGORITHM, nullptr, 0), "Open AES");
    check(BCryptSetProperty(a.h,
                            BCRYPT_CHAINING_MODE,
                            (PUCHAR)BCRYPT_CHAIN_MODE_GCM,
                            (ULONG)sizeof(BCRYPT_CHAIN_MODE_GCM),
                            0),
          "Set GCM");

    // 4) импорт ключа
    DWORD objLen = 0, res = 0;
    check(BCryptGetProperty(a.h, BCRYPT_OBJECT_LENGTH, (PUCHAR)&objLen, sizeof(objLen), &res, 0),
          "Get OBJECT_LENGTH");
    std::vector<uint8_t> keyObj(objLen);
    BCRYPT_KEY_HANDLE kh = nullptr;
    check(BCryptGenerateSymmetricKey(a.h, &kh,
                                     keyObj.data(), objLen,
                                     const_cast<PUCHAR>(key.data()),
                                     static_cast<ULONG>(key.size()),
                                     0),
          "GenerateSymmetricKey");

    // 5) параметры GCM
    BCRYPT_AUTHENTICATED_CIPHER_MODE_INFO info;
    BCRYPT_INIT_AUTH_MODE_INFO(info);
    info.pbNonce    = iv.data();
    info.cbNonce    = static_cast<ULONG>(iv.size());
    info.pbAuthData = aad.empty() ? nullptr : const_cast<PUCHAR>(aad.data());
    info.cbAuthData = static_cast<ULONG>(aad.size());
    std::vector<uint8_t> tag(GCM_TAG_LEN);
    info.pbTag = tag.data();
    info.cbTag = static_cast<ULONG>(tag.size());

    // 6) узнать длину шифртекста
    ULONG cbCipher = 0;
    check(BCryptEncrypt(kh,
                        const_cast<PUCHAR>(plaintext.data()),
                        static_cast<ULONG>(plaintext.size()),
                        &info,
                        nullptr, 0,
                        nullptr, 0,
                        &cbCipher,
                        0),
          "BCryptEncrypt(query)");

    // 7) шифрование
    std::vector<uint8_t> cipher(cbCipher);
    check(BCryptEncrypt(kh,
                        const_cast<PUCHAR>(plaintext.data()),
                        static_cast<ULONG>(plaintext.size()),
                        &info,
                        nullptr, 0,
                        cipher.data(),
                        static_cast<ULONG>(cipher.size()),
                        &cbCipher,
                        0),
          "BCryptEncrypt(run)");
    cipher.resize(cbCipher);

    // 8) сериализация BlobV1: [magic(4), lens(3), salt, iv, cipher, tag]
    std::vector<uint8_t> blob;
    blob.reserve(4 + 3 + SALT_LEN + GCM_IV_LEN + cipher.size() + tag.size());
    blob.push_back('L'); blob.push_back('C'); blob.push_back('1'); blob.push_back(0); // magic
    blob.push_back(static_cast<uint8_t>(SALT_LEN));
    blob.push_back(static_cast<uint8_t>(GCM_IV_LEN));
    blob.push_back(static_cast<uint8_t>(GCM_TAG_LEN));
    blob.insert(blob.end(), salt.begin(),   salt.end());
    blob.insert(blob.end(), iv.begin(),     iv.end());
    blob.insert(blob.end(), cipher.begin(), cipher.end());
    blob.insert(blob.end(), tag.begin(),    tag.end());
    return blob;
}

// Внутреннее: расшифровка сериализованного blob
static std::vector<uint8_t> decrypt_gcm_with_salt_blob(const std::string& secret,
                                                       const std::vector<uint8_t>& blob,
                                                       const std::vector<uint8_t>& aad = {}) {
    if (blob.size() < 4 + 3) throw std::runtime_error("blob too small");
    size_t off = 0;

    // magic
    if (!(blob[0]=='L' && blob[1]=='C' && blob[2]=='1')) throw std::runtime_error("bad magic");
    off += 4;

    const ULONG saltLen = blob[off++], ivLen = blob[off++], tagLen = blob[off++];
    if (off + saltLen + ivLen + tagLen > blob.size()) throw std::runtime_error("blob corrupt");

    const uint8_t* salt = &blob[off]; off += saltLen;
    const uint8_t* iv   = &blob[off]; off += ivLen;

    if (blob.size() < off + tagLen) throw std::runtime_error("blob corrupt");
    const size_t ctLen = blob.size() - off - tagLen;
    const uint8_t* ct  = &blob[off];
    const uint8_t* tag = &blob[off + ctLen];

    // derive key
    std::vector<uint8_t> saltVec(salt, salt + saltLen);
    auto key = derive_key_pbkdf2(secret, saltVec, PBKDF2_ITERS);

    // AES-GCM decrypt
    AlgHandle a;
    check(BCryptOpenAlgorithmProvider(&a.h, BCRYPT_AES_ALGORITHM, nullptr, 0), "Open AES");
    check(BCryptSetProperty(a.h,
                            BCRYPT_CHAINING_MODE,
                            (PUCHAR)BCRYPT_CHAIN_MODE_GCM,
                            (ULONG)sizeof(BCRYPT_CHAIN_MODE_GCM),
                            0),
          "Set GCM");

    DWORD objLen = 0, res = 0;
    check(BCryptGetProperty(a.h, BCRYPT_OBJECT_LENGTH, (PUCHAR)&objLen, sizeof(objLen), &res, 0),
          "Get OBJECT_LENGTH");
    std::vector<uint8_t> keyObj(objLen);
    BCRYPT_KEY_HANDLE kh = nullptr;
    check(BCryptGenerateSymmetricKey(a.h, &kh,
                                     keyObj.data(), objLen,
                                     const_cast<PUCHAR>(key.data()),
                                     static_cast<ULONG>(key.size()),
                                     0),
          "GenerateSymmetricKey");

    BCRYPT_AUTHENTICATED_CIPHER_MODE_INFO info;
    BCRYPT_INIT_AUTH_MODE_INFO(info);
    info.pbNonce    = const_cast<PUCHAR>(iv);
    info.cbNonce    = ivLen;
    info.pbAuthData = aad.empty() ? nullptr : const_cast<PUCHAR>(aad.data());
    info.cbAuthData = static_cast<ULONG>(aad.size());
    std::vector<uint8_t> tagBuf(tag, tag + tagLen);
    info.pbTag = tagBuf.data();
    info.cbTag = tagLen;

    ULONG ptLen = 0;
    check(BCryptDecrypt(kh,
                        const_cast<PUCHAR>(ct), static_cast<ULONG>(ctLen),
                        &info,
                        nullptr, 0,
                        nullptr, 0, &ptLen,
                        0),
          "BCryptDecrypt(query)");

    std::vector<uint8_t> pt(ptLen);
    check(BCryptDecrypt(kh,
                        const_cast<PUCHAR>(ct), static_cast<ULONG>(ctLen),
                        &info,
                        nullptr, 0,
                        pt.data(), ptLen, &ptLen,
                        0),
          "BCryptDecrypt(run)");
    pt.resize(ptLen);
    return pt;
}

// --- публичный API ---
EncryptedBlob encrypt(const std::string& secret,
                      const std::vector<uint8_t>& plaintext) {
    EncryptedBlob out;
    out.data = encrypt_gcm_with_salt_blob(secret, plaintext);
    return out;
}

std::vector<uint8_t> decrypt(const std::string& secret,
                             const std::vector<uint8_t>& blob) {
    return decrypt_gcm_with_salt_blob(secret, blob);
}

} // namespace crypto
