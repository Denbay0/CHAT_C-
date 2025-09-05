#pragma once
#include <string>
#include <vector>
#include <mutex>
#include <fstream>
#include <cstdint>

namespace lanchat {

struct Message {
  uint64_t ts_ms;
  std::string user;
  std::string text;
  std::string hash_hex;
};

class Storage {
public:
  Storage(std::size_t last_cap);
  ~Storage();

  bool open(const std::string& data_dir);
  void append(const Message& m);
  std::vector<Message> last(std::size_t n);

private:
  std::mutex mx_;
  std::vector<Message> ring_;
  std::size_t cap_;
  std::ofstream log_;
};

} // namespace lanchat

