#pragma once
#include <string>
#include <vector>
#include <mutex>
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
  std::vector<Message> last(std::size_t n);
  void append(const Message& m);

private:
  std::mutex mx_;
  std::vector<Message> ring_;
  std::size_t cap_;
};

} // namespace lanchat
