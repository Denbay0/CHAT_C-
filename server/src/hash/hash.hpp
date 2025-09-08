#pragma once
#include <string>
#include <cstdint>

namespace lanchat {

uint64_t fnv1a64(const std::string& data);
std::string hex64(uint64_t x);

} // namespace lanchat
