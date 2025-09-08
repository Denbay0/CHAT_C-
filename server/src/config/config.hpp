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

  // Шифрование логов (AES-GCM)
  bool        enc_enabled = false;
  std::string enc_key_hex; // 64 hex-символа = 32 байта
};

// CLI
void print_usage(const char* argv0);
void parse_args(int argc, char** argv, Config& cfg);

// Конфиг-файл (data/server.ini)
bool load_config_file(const std::string& path, Config& cfg);
bool save_config_file(const std::string& path, const Config& cfg);

// Автозапуск: если argc==1 → создаёт data/server.ini при необходимости,
// генерит secret/enc-key, грузит cfg. Если есть CLI-аргументы — они парсятся
// и перекрывают значения из файла.
void bootstrap_auto_config(int argc, char** argv, Config& cfg);

} // namespace lanchat
