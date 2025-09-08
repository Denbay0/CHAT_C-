#define NOMINMAX
#include <windows.h>
#include <bcrypt.h>
#pragma comment(lib, "bcrypt.lib")

#include <cstdint>
#include <vector>
#include <string>
#include <stdexcept>
#include <iostream>
#include <sstream>
#include <iomanip>

// Утилита для проверки NTSTATUS
static inline void NT(NTSTATUS s, const char* where) {
    if (!BCRYPT_SUCCESS(s)) {
        throw std::runtime_error(std::string(where) + " failed (NTSTATUS=" + std::to_string(s) + ")");
    }
}

struct CipherBlob {
    std::vector<uint8_t> iv;   // 12 bytes (nonce)
    std::vector<uint8_t> ct;   // ciphertext
    std::vector<uint8_t> tag;  // 16 bytes (auth tag)
};

static const wchar_t ALG_AES[]      = BCRYPT_AES_ALGORITHM;
static const wchar_t PROP_CHAINING[] = BCRYPT_CHAINING_MODE;
static const wchar_t MODE_GCM[]      = BCRYPT_CHAIN_MODE_GCM;

// Открываем провайдер AES и включаем режим GCM
BCRYPT_ALG_HANDLE open_aes_gcm() {
    BCRYPT_ALG_HANDLE hAlg = nullptr;
    NT(BCryptOpenAlgorithmProvider(&hAlg, ALG_AES, nullptr, 0), "BCryptOpenAlgorithmProvider");
    NT(BCryptSetProperty(hAlg, PROP_CHAINING,
                         (PUCHAR)MODE_GCM,
                         (ULONG)(sizeof(wchar_t) * (wcslen(MODE_GCM) + 1)),
                         0), "BCryptSetProperty(CHAINING_MODE=GCM)");
    return hAlg;
}

// Криптостойкий ключ/IV: системный RNG CNG (SP800-90)
std::vector<uint8_t> gen_bytes(size_t n) {
    std::vector<uint8_t> v(n);
    NT(BCryptGenRandom(nullptr, v.data(), (ULONG)v.size(), BCRYPT_USE_SYSTEM_PREFERRED_RNG),
       "BCryptGenRandom");
    return v;
}

CipherBlob aes_gcm_encrypt(const std::vector<uint8_t>& key,
                           const std::vector<uint8_t>& plain,
                           const std::vector<uint8_t>& aad = {}) {
    if (key.size() != 32) throw std::runtime_error("AES-256 key must be 32 bytes");

    CipherBlob out;
    out.iv  = gen_bytes(12);   // стандартный размер nonce для GCM
    out.tag = std::vector<uint8_t>(16);
    out.ct  = std::vector<uint8_t>(plain.size());

    BCRYPT_ALG_HANDLE hAlg = open_aes_gcm();
    BCRYPT_KEY_HANDLE hKey = nullptr;

    DWORD keyObjLen = 0, cbRes = 0;
    NT(BCryptGetProperty(hAlg, BCRYPT_OBJECT_LENGTH, (PUCHAR)&keyObjLen, sizeof(keyObjLen), &cbRes, 0),
       "BCryptGetProperty(OBJECT_LENGTH)");
    std::vector<uint8_t> keyObj(keyObjLen);

    NT(BCryptGenerateSymmetricKey(hAlg, &hKey,
                                  keyObj.data(), keyObjLen,
                                  (PUCHAR)key.data(), (ULONG)key.size(), 0),
       "BCryptGenerateSymmetricKey");

    BCRYPT_AUTHENTICATED_CIPHER_MODE_INFO ainfo;
    BCRYPT_INIT_AUTH_MODE_INFO(ainfo);
    ainfo.pbNonce    = out.iv.data();
    ainfo.cbNonce    = (ULONG)out.iv.size();
    ainfo.pbAuthData = aad.empty() ? nullptr : const_cast<PUCHAR>(aad.data());
    ainfo.cbAuthData = (ULONG)aad.size();
    ainfo.pbTag      = out.tag.data();
    ainfo.cbTag      = (ULONG)out.tag.size();

    ULONG cbDone = 0;
    NT(BCryptEncrypt(hKey,
                     const_cast<PUCHAR>(plain.data()), (ULONG)plain.size(),
                     &ainfo,
                     nullptr, 0,
                     out.ct.data(), (ULONG)out.ct.size(),
                     &cbDone, 0),
       "BCryptEncrypt");
    out.ct.resize(cbDone);

    if (hKey) BCryptDestroyKey(hKey);
    if (hAlg) BCryptCloseAlgorithmProvider(hAlg, 0);
    return out;
}

std::vector<uint8_t> aes_gcm_decrypt(const std::vector<uint8_t>& key,
                                     const CipherBlob& blob,
                                     const std::vector<uint8_t>& aad = {}) {
    if (key.size() != 32 || blob.iv.size() != 12 || blob.tag.size() != 16)
        throw std::runtime_error("Invalid key/iv/tag sizes");

    BCRYPT_ALG_HANDLE hAlg = open_aes_gcm();
    BCRYPT_KEY_HANDLE hKey = nullptr;

    DWORD keyObjLen = 0, cbRes = 0;
    NT(BCryptGetProperty(hAlg, BCRYPT_OBJECT_LENGTH, (PUCHAR)&keyObjLen, sizeof(keyObjLen), &cbRes, 0),
       "BCryptGetProperty(OBJECT_LENGTH)");
    std::vector<uint8_t> keyObj(keyObjLen);

    NT(BCryptGenerateSymmetricKey(hAlg, &hKey,
                                  keyObj.data(), keyObjLen,
                                  (PUCHAR)key.data(), (ULONG)key.size(), 0),
       "BCryptGenerateSymmetricKey");

    std::vector<uint8_t> plain(blob.ct.size());

    BCRYPT_AUTHENTICATED_CIPHER_MODE_INFO ainfo;
    BCRYPT_INIT_AUTH_MODE_INFO(ainfo);
    ainfo.pbNonce    = const_cast<PUCHAR>(blob.iv.data());
    ainfo.cbNonce    = (ULONG)blob.iv.size();
    ainfo.pbAuthData = aad.empty() ? nullptr : const_cast<PUCHAR>(aad.data());
    ainfo.cbAuthData = (ULONG)aad.size();
    ainfo.pbTag      = const_cast<PUCHAR>(blob.tag.data());
    ainfo.cbTag      = (ULONG)blob.tag.size();

    ULONG cbDone = 0;
    NT(BCryptDecrypt(hKey,
                     (PUCHAR)blob.ct.data(), (ULONG)blob.ct.size(),
                     &ainfo,
                     nullptr, 0,
                     plain.data(), (ULONG)plain.size(),
                     &cbDone, 0),
       "BCryptDecrypt");
    plain.resize(cbDone);

    if (hKey) BCryptDestroyKey(hKey);
    if (hAlg) BCryptCloseAlgorithmProvider(hAlg, 0);
    return plain;
}

// HEX вывод (нужны <sstream> и <iomanip>)
std::string toHex(const std::vector<uint8_t>& v) {
    std::ostringstream ss;
    ss << std::hex << std::setfill('0');   // формат 16ричный + заполнение нулями
    for (auto b : v) ss << std::setw(2) << static_cast<int>(b); // 2 символа на байт
    return ss.str();
}

int main() {
    try {
        // В реальном приложении ключ хранится в секрете (KMS/DPAPI/env). Здесь — просто генерим для демо:
        auto key = gen_bytes(32); // AES-256

        std::cout << "--- AES-GCM (CNG) demo ---\n";
        std::cout << "Введите сообщение: ";
        std::string input;
        std::getline(std::cin, input);

        // (опционально) AAD — доп. данные для аутентификации (например, user_id|timestamp)
        std::vector<uint8_t> aad; // пусто в демо

        // Шифрование
        CipherBlob blob = aes_gcm_encrypt(key, std::vector<uint8_t>(input.begin(), input.end()), aad);

        std::cout << "\nIV:  "  << toHex(blob.iv)  << "\n";
        std::cout << "CT:  "    << toHex(blob.ct)  << "\n";
        std::cout << "TAG: "    << toHex(blob.tag) << "\n";

        // Дешифрование
        auto rec = aes_gcm_decrypt(key, blob, aad);
        std::string recovered(rec.begin(), rec.end());
        std::cout << "\nРасшифровано: " << recovered << "\n";
    } catch (const std::exception& e) {
        std::cerr << "ERROR: " << e.what() << "\n";
        return 1;
    }
    return 0;
}
