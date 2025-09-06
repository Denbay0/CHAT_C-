#pragma once
#include <string>
#include <cstddef>
#include <cstdint>

namespace lanchat {

struct Config {
  std::string bind_addr = "0.0.0.0";
  uint16_t port = 5555;
  std::string data_dir = "data";
  std::string secret = "changeme";
  std::size_t history_on_join = 20;
};

void parse_args(int argc, char** argv, Config& cfg);
void print_usage(const char* argv0);

} // namespace lanchat
