#pragma once
#include <string>
#include <vector>
#include <mutex>
#include <fstream>
#include <unordered_set>
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

  // открыть data_dir и файл messages.log (создастся при необходимости)
  bool open(const std::string& data_dir);

  // загрузить последние max_lines строк из messages.log в буфер,
  // параллельно собрать множ-во пользователей
  bool load_from_log(std::size_t max_lines, std::unordered_set<std::string>& users_out);

  // добавить сообщение (и в память, и в файл)
  void append(const Message& m);

  // последние n сообщений (из памяти)
  std::vector<Message> last(std::size_t n);

private:
  std::mutex mx_;
  std::vector<Message> ring_;
  std::size_t cap_;
  std::string data_dir_;
  std::ofstream log_;
};

} // namespace lanchat
