#pragma once
#include <string>
#include <vector>
#include <optional>
#include <cstdint>

struct sqlite3;

namespace lanchat {

struct DbMessage {
  uint64_t ts_ms;
  std::string user;
  std::string text;
  std::string hash_hex;
  std::optional<std::string> recipient; // nullopt = общий чат
};

class Db {
public:
  Db() = default;
  ~Db();

  bool open(const std::string& path);          // открывает и вызывает init()
  void close();
  bool init();                                 // создаёт таблицы если их нет

  // пользователи
  int64_t ensure_user(const std::string& username);  // вернёт id (создаст если нет)
  std::optional<int64_t> get_user_id(const std::string& username);

  // сообщения
  bool insert_message(int64_t sender_id,
                      std::optional<int64_t> recipient_id,
                      uint64_t ts_ms,
                      const std::string& text,
                      const std::string& hash_hex);

  std::vector<DbMessage> last_messages(std::size_t limit,
                                       std::optional<int64_t> room_recipient = std::nullopt);

private:
  sqlite3* db_ = nullptr;
};

} // namespace lanchat
