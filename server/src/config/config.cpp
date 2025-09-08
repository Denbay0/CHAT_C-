#include "util/utils.hpp"
#include "config.hpp"

#include <filesystem>
#include <iostream>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#ifdef _WIN32
  #define NOMINMAX
  #include <windows.h>
  #include <bcrypt.h>
  #pragma comment(lib, "bcrypt.lib")
#endif

namespace lanchat {

static std::string default_ini_path(){
  return std::string("data") + "/" + "server.ini";
}

static std::vector<uint8_t> secure_random_bytes(size_t n){
  std::vector<uint8_t> v(n, 0);
#ifdef _WIN32
  if (BCryptGenRandom(nullptr, v.data(), (ULONG)v.size(), BCRYPT_USE_SYSTEM_PREFERRED_RNG) != 0){
    for(size_t i=0;i<n;++i) v[i] = (uint8_t)rand();
  }
#else
  std::ifstream ur("/dev/urandom", std::ios::binary);
  if (ur.good()){
    ur.read(reinterpret_cast<char*>(v.data()), (std::streamsize)n);
    if (ur.gcount() == (std::streamsize)n) return v;
  }
  for(size_t i=0;i<n;++i) v[i] = (uint8_t)rand();
#endif
  return v;
}

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

static void trim(std::string& s){
  size_t a = s.find_first_not_of(" \t\r\n");
  size_t b = s.find_last_not_of(" \t\r\n");
  if (a==std::string::npos){ s.clear(); return; }
  s = s.substr(a, b-a+1);
}

bool load_config_file(const std::string& path, Config& cfg){
  std::ifstream in(path);
  if (!in.is_open()) return false;

  std::string line;
  while (std::getline(in, line)){
    std::string raw = line;
    trim(line);
    if (line.empty()) continue;
    if (line[0]=='#' || line[0]==';') continue;

    size_t eq = line.find('=');
    if (eq==std::string::npos) continue;
    std::string key = line.substr(0, eq);
    std::string val = line.substr(eq+1);
    trim(key); trim(val);
    if (key=="bind") cfg.bind_addr = val;
    else if (key=="port"){ try{ cfg.port = static_cast<uint16_t>(std::stoi(val)); } catch(...){} }
    else if (key=="data") cfg.data_dir = val;
    else if (key=="secret") cfg.secret = val;
    else if (key=="hist"){ try{ cfg.history_on_join = static_cast<std::size_t>(std::stoul(val)); } catch(...){} }
    else if (key=="enc_key_hex"){
      cfg.enc_key_hex = val;
      cfg.enc_enabled = (val.size()==64);
    }
  }
  return true;
}

bool save_config_file(const std::string& path, const Config& cfg){
  std::filesystem::create_directories(std::filesystem::path(path).parent_path());
  std::ofstream out(path, std::ios::trunc);
  if (!out.is_open()) return false;

  out << "# LAN Chat Server config\n";
  out << "bind=" << cfg.bind_addr << "\n";
  out << "port=" << cfg.port << "\n";
  out << "data=" << cfg.data_dir << "\n";
  out << "secret=" << cfg.secret << "\n";
  out << "hist=" << cfg.history_on_join << "\n";
  if (cfg.enc_enabled && cfg.enc_key_hex.size()==64)
    out << "enc_key_hex=" << cfg.enc_key_hex << "\n";
  else
    out << "enc_key_hex=\n";
  out.flush();
  return true;
}

void bootstrap_auto_config(int argc, char** argv, Config& cfg){

  const std::string ini = default_ini_path();
  bool ini_loaded = load_config_file(ini, cfg);

  if (argc > 1){
    parse_args(argc, argv, cfg);
  }

  if (!ini_loaded){
    std::filesystem::create_directories(cfg.data_dir);
  }

  if (cfg.secret == "changeme"){
    auto s = secure_random_bytes(16);
    cfg.secret = hex_encode(s);
  }

  if (!cfg.enc_enabled || cfg.enc_key_hex.size()!=64){
    auto k = secure_random_bytes(32);
    cfg.enc_key_hex = hex_encode(k);
    cfg.enc_enabled = true;
  }

  save_config_file(ini, cfg);

  std::cout << "Config: bind=" << cfg.bind_addr
            << " port=" << cfg.port
            << " data=" << cfg.data_dir
            << " hist=" << cfg.history_on_join
            << " enc=" << (cfg.enc_enabled ? "on" : "off")
            << "\n";
}

}
