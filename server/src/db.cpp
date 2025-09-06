#include "db.hpp"
#include <sqlite3.h>
#include <filesystem>
#include <cassert>

namespace lanchat {

Db::~Db(){ close(); }

bool Db::open(const std::string& path){
  std::filesystem::create_directories(std::filesystem::path(path).parent_path());
  if (sqlite3_open(path.c_str(), &db_) != SQLITE_OK) return false;
  // Включаем foreign keys
  sqlite3_exec(db_, "PRAGMA foreign_keys = ON;", nullptr, nullptr, nullptr);
  return init();
}

void Db::close(){
  if (db_) { sqlite3_close(db_); db_ = nullptr; }
}

bool Db::init(){
  const char* sql =
    "CREATE TABLE IF NOT EXISTS users ("
    "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
    "  username TEXT NOT NULL UNIQUE"
    ");"
    "CREATE TABLE IF NOT EXISTS messages ("
    "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
    "  ts_ms INTEGER NOT NULL,"
    "  sender_id INTEGER NOT NULL,"
    "  recipient_id INTEGER NULL,"
    "  text TEXT NOT NULL,"
    "  hash_hex TEXT NOT NULL,"
    "  FOREIGN KEY(sender_id) REFERENCES users(id) ON DELETE CASCADE,"
    "  FOREIGN KEY(recipient_id) REFERENCES users(id) ON DELETE CASCADE"
    ");"
    "CREATE INDEX IF NOT EXISTS idx_messages_ts ON messages(ts_ms DESC);";
  return sqlite3_exec(db_, sql, nullptr, nullptr, nullptr) == SQLITE_OK;
}

std::optional<int64_t> Db::get_user_id(const std::string& username){
  const char* sql = "SELECT id FROM users WHERE username = ?;";
  sqlite3_stmt* st=nullptr;
  if (sqlite3_prepare_v2(db_, sql, -1, &st, nullptr)!=SQLITE_OK) return std::nullopt;
  sqlite3_bind_text(st, 1, username.c_str(), -1, SQLITE_TRANSIENT);
  std::optional<int64_t> out;
  if (sqlite3_step(st) == SQLITE_ROW){
    out = sqlite3_column_int64(st, 0);
  }
  sqlite3_finalize(st);
  return out;
}

int64_t Db::ensure_user(const std::string& username){
  if (auto id = get_user_id(username)) return *id;
  const char* ins = "INSERT INTO users(username) VALUES(?);";
  sqlite3_stmt* st=nullptr;
  if (sqlite3_prepare_v2(db_, ins, -1, &st, nullptr)!=SQLITE_OK) return -1;
  sqlite3_bind_text(st, 1, username.c_str(), -1, SQLITE_TRANSIENT);
  if (sqlite3_step(st) != SQLITE_DONE){ sqlite3_finalize(st); return -1; }
  sqlite3_finalize(st);
  return sqlite3_last_insert_rowid(db_);
}

bool Db::insert_message(int64_t sender_id,
                        std::optional<int64_t> recipient_id,
                        uint64_t ts_ms,
                        const std::string& text,
                        const std::string& hash_hex){
  const char* ins =
    "INSERT INTO messages(ts_ms, sender_id, recipient_id, text, hash_hex)"
    " VALUES(?,?,?,?,?);";
  sqlite3_stmt* st=nullptr;
  if (sqlite3_prepare_v2(db_, ins, -1, &st, nullptr)!=SQLITE_OK) return false;
  sqlite3_bind_int64(st, 1, (sqlite3_int64)ts_ms);
  sqlite3_bind_int64(st, 2, (sqlite3_int64)sender_id);
  if (recipient_id) sqlite3_bind_int64(st, 3, (sqlite3_int64)*recipient_id);
  else sqlite3_bind_null(st, 3);
  sqlite3_bind_text(st, 4, text.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(st, 5, hash_hex.c_str(), -1, SQLITE_TRANSIENT);
  bool ok = sqlite3_step(st) == SQLITE_DONE;
  sqlite3_finalize(st);
  return ok;
}

std::vector<DbMessage> Db::last_messages(std::size_t limit,
                                         std::optional<int64_t> room_recipient){
  std::vector<DbMessage> out;
  const char* sel_global =
    "SELECT m.ts_ms, u.username, m.text, m.hash_hex, NULL as recipient_name "
    "FROM messages m "
    "JOIN users u ON u.id = m.sender_id "
    "WHERE m.recipient_id IS NULL "
    "ORDER BY m.ts_ms DESC LIMIT ?;";
  const char* sel_dm =
    "SELECT m.ts_ms, su.username, m.text, m.hash_hex, ru.username as recipient_name "
    "FROM messages m "
    "JOIN users su ON su.id = m.sender_id "
    "JOIN users ru ON ru.id = m.recipient_id "
    "WHERE m.recipient_id = ? "
    "ORDER BY m.ts_ms DESC LIMIT ?;";

  sqlite3_stmt* st=nullptr;
  if (room_recipient){
    if (sqlite3_prepare_v2(db_, sel_dm, -1, &st, nullptr)!=SQLITE_OK) return out;
    sqlite3_bind_int64(st, 1, (sqlite3_int64)*room_recipient);
    sqlite3_bind_int64(st, 2, (sqlite3_int64)limit);
  } else {
    if (sqlite3_prepare_v2(db_, sel_global, -1, &st, nullptr)!=SQLITE_OK) return out;
    sqlite3_bind_int64(st, 1, (sqlite3_int64)limit);
  }

  while (sqlite3_step(st) == SQLITE_ROW){
    DbMessage m{};
    m.ts_ms = (uint64_t)sqlite3_column_int64(st, 0);
    m.user  = (const char*)sqlite3_column_text(st, 1);
    m.text  = (const char*)sqlite3_column_text(st, 2);
    m.hash_hex = (const char*)sqlite3_column_text(st, 3);
    if (sqlite3_column_type(st, 4) != SQLITE_NULL){
      m.recipient = std::string((const char*)sqlite3_column_text(st, 4));
    }
    out.push_back(std::move(m));
  }
  sqlite3_finalize(st);

  // вернуть по возрастанию времени (как мы раньше делали):
  std::reverse(out.begin(), out.end());
  return out;
}

} // namespace lanchat
