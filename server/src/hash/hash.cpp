#include "hash/hash.hpp"

#include <string>

namespace lanchat {

uint64_t fnv1a64(const std::string& data){
  const uint64_t FNV_OFFSET = 1469598103934665603ULL;
  const uint64_t FNV_PRIME  = 1099511628211ULL;
  uint64_t h = FNV_OFFSET;
  for (unsigned char c: data){ h ^= c; h *= FNV_PRIME; }
  return h;
}

std::string hex64(uint64_t x){
  static const char* hexd="0123456789abcdef";
  std::string s(16,'0');
  for(int i=15;i>=0;--i){ s[i]=hexd[x & 0xF]; x >>= 4; }
  return s;
}

}
