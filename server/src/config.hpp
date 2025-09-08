#pragma once
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

  // шифрование логов (опционально)
  bool        enc_enabled = false;
  std::string enc_key_hex; // 64 hex-символа = 32 байта
};

// Печать помощи
void print_usage(const char* argv0);

// Парсер аргументов командной строки (заполняет cfg)
void parse_args(int argc, char** argv, Config& cfg);

} // namespace lanchat
