#include "config.hpp"
#include <iostream>
#include <string>
#include <cstdlib>

namespace lanchat {

void print_usage(const char* argv0){
  std::cout <<
    "LAN Chat Server\n"
    "Usage: " << argv0 <<
    " [--bind 0.0.0.0]"
    " [--port 5555]"
    " [--data ./data]"
    " [--secret KEY]"
    " [--hist 20]"
    " [--enc-key-hex <64hex>]\n";
}

void parse_args(int argc, char** argv, Config& cfg){
  for (int i = 1; i < argc; ++i){
    std::string a = argv[i];
    auto next = [&](const char* err)->std::string{
      if (i+1 >= argc){ std::cerr << err << "\n"; std::exit(1); }
      return argv[++i];
    };

    if      (a == "--bind")   cfg.bind_addr = next("missing --bind value");
    else if (a == "--port")   cfg.port = static_cast<uint16_t>(std::stoi(next("missing --port value")));
    else if (a == "--data")   cfg.data_dir = next("missing --data value");
    else if (a == "--secret") cfg.secret   = next("missing --secret value");
    else if (a == "--hist")   cfg.history_on_join = static_cast<std::size_t>(std::stoul(next("missing --hist value")));
    else if (a == "--enc-key-hex"){
      cfg.enc_key_hex = next("missing --enc-key-hex value");
      if (cfg.enc_key_hex.size() == 64) cfg.enc_enabled = true;
      else { std::cerr << "--enc-key-hex must be 64 hex chars (32 bytes)\n"; std::exit(1); }
    }
    else if (a == "-h" || a == "--help") {
      print_usage(argv[0]); std::exit(0);
    }
    else {
      std::cerr << "Unknown arg: " << a << "\n";
      print_usage(argv[0]); std::exit(1);
    }
  }
}

} // namespace lanchat
