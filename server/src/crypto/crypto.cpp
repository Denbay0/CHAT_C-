#include "crypto/crypto.hpp"
#include <stdexcept>
#include <string>
#include <vector>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <stdexcept>
#include <string>

#ifdef _WIN32
  #define NOMINMAX
  #include <windows.h>
  #include <bcrypt.h>
  #pragma comment(lib, "bcrypt.lib")
#endif



namespace lanchat {

#ifdef _WIN32

static inline void NT(NTSTATUS s, const char* where){
  if (!BCRYPT_SUCCESS(s)){
    throw std::runtime_error(std::string(where) + " failed (NTSTATUS=" + std::to_string(s) + ")");
  }
}
static BCRYPT_ALG_HANDLE open_aes_gcm(){
  BCRYPT_ALG_HANDLE hAlg = nullptr;
  NT(BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_AES_ALGORITHM, nullptr, 0), "BCryptOpenAlgorithmProvider");
  NT(BCryptSetProperty(hAlg, BCRYPT_CHAINING_MODE,
                       (PUCHAR)BCRYPT_CHAIN_MODE_GCM,
                       (ULONG)(sizeof(wchar_t) * (wcslen(BCRYPT_CHAIN_MODE_GCM)+1)),
                       0), "BCryptSetProperty(GCM)");
  return hAlg;
}
static std::vector<uint8_t> gen_bytes(size_t n){
  std::vector<uint8_t> v(n);
  NT(BCryptGenRandom(nullptr, v.data(), (ULONG)v.size(), BCRYPT_USE_SYSTEM_PREFERRED_RNG),
     "BCryptGenRandom");
  return v;
}

GcmBlob aes_gcm_encrypt_win(const std::vector<uint8_t>& key32,
                            const std::vector<uint8_t>& plain,
                            const std::vector<uint8_t>& aad){
  if (key32.size()!=32) throw std::runtime_error("AES-256 key must be 32 bytes");
  GcmBlob out;
  out.iv  = gen_bytes(12);
  out.tag = std::vector<uint8_t>(16);
  out.ct  = std::vector<uint8_t>(plain.size());

  BCRYPT_ALG_HANDLE hAlg = open_aes_gcm();
  BCRYPT_KEY_HANDLE hKey = nullptr;

  DWORD keyObjLen=0, cbRes=0;
  NT(BCryptGetProperty(hAlg, BCRYPT_OBJECT_LENGTH, (PUCHAR)&keyObjLen, sizeof(keyObjLen), &cbRes, 0),
     "GetProperty(OBJECT_LENGTH)");
  std::vector<uint8_t> keyObj(keyObjLen);

  NT(BCryptGenerateSymmetricKey(hAlg, &hKey, keyObj.data(), keyObjLen,
                                (PUCHAR)key32.data(), (ULONG)key32.size(), 0),
     "GenerateSymmetricKey");

  BCRYPT_AUTHENTICATED_CIPHER_MODE_INFO ainfo;
  BCRYPT_INIT_AUTH_MODE_INFO(ainfo);
  ainfo.pbNonce = out.iv.data();
  ainfo.cbNonce = (ULONG)out.iv.size();
  ainfo.pbAuthData = aad.empty()? nullptr : (PUCHAR)aad.data();
  ainfo.cbAuthData = (ULONG)aad.size();
  ainfo.pbTag = out.tag.data();
  ainfo.cbTag = (ULONG)out.tag.size();

  ULONG done=0;
  NT(BCryptEncrypt(hKey,
                   (PUCHAR)plain.data(), (ULONG)plain.size(),
                   &ainfo,
                   nullptr, 0,
                   out.ct.data(), (ULONG)out.ct.size(),
                   &done, 0),
     "Encrypt");
  out.ct.resize(done);

  if (hKey) BCryptDestroyKey(hKey);
  if (hAlg) BCryptCloseAlgorithmProvider(hAlg, 0);
  return out;
}

std::vector<uint8_t> aes_gcm_decrypt_win(const std::vector<uint8_t>& key32,
                                         const GcmBlob& blob,
                                         const std::vector<uint8_t>& aad){
  if (key32.size()!=32 || blob.iv.size()!=12 || blob.tag.size()!=16)
    throw std::runtime_error("Invalid key/iv/tag sizes");

  BCRYPT_ALG_HANDLE hAlg = open_aes_gcm();
  BCRYPT_KEY_HANDLE hKey = nullptr;

  DWORD keyObjLen=0, cbRes=0;
  NT(BCryptGetProperty(hAlg, BCRYPT_OBJECT_LENGTH, (PUCHAR)&keyObjLen, sizeof(keyObjLen), &cbRes, 0),
     "GetProperty(OBJECT_LENGTH)");
  std::vector<uint8_t> keyObj(keyObjLen);

  NT(BCryptGenerateSymmetricKey(hAlg, &hKey, keyObj.data(), keyObjLen,
                                (PUCHAR)key32.data(), (ULONG)key32.size(), 0),
     "GenerateSymmetricKey");

  std::vector<uint8_t> plain(blob.ct.size());

  BCRYPT_AUTHENTICATED_CIPHER_MODE_INFO ainfo;
  BCRYPT_INIT_AUTH_MODE_INFO(ainfo);
  ainfo.pbNonce = (PUCHAR)blob.iv.data();
  ainfo.cbNonce = (ULONG)blob.iv.size();
  ainfo.pbAuthData = aad.empty()? nullptr : (PUCHAR)aad.data();
  ainfo.cbAuthData = (ULONG)aad.size();
  ainfo.pbTag = (PUCHAR)blob.tag.data();
  ainfo.cbTag = (ULONG)blob.tag.size();

  ULONG done=0;
  NT(BCryptDecrypt(hKey,
                   (PUCHAR)blob.ct.data(), (ULONG)blob.ct.size(),
                   &ainfo,
                   nullptr, 0,
                   plain.data(), (ULONG)plain.size(),
                   &done, 0),
     "Decrypt");
  plain.resize(done);

  if (hKey) BCryptDestroyKey(hKey);
  if (hAlg) BCryptCloseAlgorithmProvider(hAlg, 0);
  return plain;
}

#else

// не-Windows заглушки, чтобы проект собирался
GcmBlob aes_gcm_encrypt_win(const std::vector<uint8_t>&, const std::vector<uint8_t>&, const std::vector<uint8_t>&){
  throw std::runtime_error("AES-GCM (Windows CNG) not available on this platform");
}
std::vector<uint8_t> aes_gcm_decrypt_win(const std::vector<uint8_t>&, const GcmBlob&, const std::vector<uint8_t>&){
  throw std::runtime_error("AES-GCM (Windows CNG) not available on this platform");
}

#endif

} // namespace lanchat
