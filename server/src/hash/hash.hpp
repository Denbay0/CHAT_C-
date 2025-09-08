#ifndef LANCHAT_HASH_HASH_HPP
#define LANCHAT_HASH_HASH_HPP

#include <cstdint>
#include <string>

namespace lanchat {

uint64_t fnv1a64(const std::string& data);
std::string hex64(uint64_t x);

}

#endif