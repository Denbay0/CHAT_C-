#ifndef LANCHAT_STORAGE_STORAGE_HPP
#define LANCHAT_STORAGE_STORAGE_HPP

#include <cstdint>
#include <string>
#include <vector>
#include <unordered_set>
#include <mutex>
#include <fstream>

namespace lanchat {

struct Message {
  uint64_t    ts_ms = 0;
  std::string user;
  std::string text;
  std::string hash_hex;
};

struct GcmBlob {
  std::vector<uint8_t> iv;
  std::vector<uint8_t> tag;
  std::vector<uint8_t> ct;
};

class Storage {
public:
  explicit Storage(std::size_t last_cap);

  bool open(const std::string& data_dir);

  bool load_from_log(std::size_t max_lines, std::unordered_set<std::string>& users_out);

  void append(const Message& m);

  std::vector<Message> last(std::size_t n);

  inline void enable_encryption(const std::vector<uint8_t>& key){
    enc_key_ = key;
    enc_enabled_ = (enc_key_.size() == 32);
  }

private:
  std::size_t        cap_;
  std::string        data_dir_;
  std::ofstream      log_;
  std::vector<Message> ring_;
  std::mutex         mx_;

  bool               enc_enabled_ = false;
  std::vector<uint8_t> enc_key_;
};

}

#endif
