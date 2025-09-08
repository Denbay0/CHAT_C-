#ifndef LANCHAT_CONFIG_CONFIG_HPP
#define LANCHAT_CONFIG_CONFIG_HPP

#include <string>
#include <cstddef>
#include <cstdint>

namespace lanchat {

struct Config {
  std::string bind_addr = "0.0.0.0";
  uint16_t    port = 5555;
  std::string data_dir = "data";
  std::string secret = "changeme";
  std::size_t history_on_join = 20;

  bool        enc_enabled = false;
  std::string enc_key_hex;
};

void print_usage(const char* argv0);
void parse_args(int argc, char** argv, Config& cfg);

bool load_config_file(const std::string& path, Config& cfg);
bool save_config_file(const std::string& path, const Config& cfg);

void bootstrap_auto_config(int argc, char** argv, Config& cfg);

}

#endif