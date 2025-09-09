#include "storage/storage.hpp"
#include "crypto/crypto.hpp"
#include "util/utils.hpp"
#include "hash/hash.hpp"

#include <filesystem>
#include <fstream>
#include <mutex>
#include <vector>
#include <string>

namespace lanchat
{

// распознаём старый формат "GCM:..." — такие строки просто пропускаем
static bool is_legacy_gcm_line(const std::string& s) {
  return s.rfind("GCM:", 0) == 0;
}

// новый формат: "BLOB:<hex(serialized blob)>"
static bool parse_blob_hex(const std::string& s, std::vector<uint8_t>& out_blob) {
  if (s.rfind("BLOB:", 0) != 0) return false;
  const std::string hex = s.substr(5);
  return hex_decode(hex, out_blob);
}

Storage::Storage(std::size_t last_cap) : cap_(last_cap) {}

bool Storage::open(const std::string& data_dir) {
  data_dir_ = data_dir;
  std::filesystem::create_directories(data_dir_);
  const auto p = std::filesystem::path(data_dir_) / "messages.log";
  log_.open(p.string(), std::ios::app);
  return log_.is_open();
}

bool Storage::load_from_log(std::size_t max_lines,
                            std::unordered_set<std::string>& users_out) {
  const auto p = std::filesystem::path(data_dir_) / "messages.log";
  if (!std::filesystem::exists(p)) return true;

  std::vector<std::string> lines;
  {
    std::ifstream in(p.string());
    if (!in.is_open()) return false;
    std::string line;
    while (std::getline(in, line)) {
      lines.push_back(std::move(line));
      if (lines.size() > max_lines) lines.erase(lines.begin());
    }
  }

  for (const auto& line : lines) {
    // ts \t user \t payload \t hash
    std::vector<std::string> cols; cols.reserve(4);
    std::string cur; cur.reserve(line.size());
    for (char c : line) {
      if (c=='\t'){ cols.push_back(cur); cur.clear(); }
      else cur.push_back(c);
    }
    cols.push_back(cur);
    if (cols.size() < 4) continue;

    Message m{};
    try { m.ts_ms = static_cast<uint64_t>(std::stoull(cols[0])); }
    catch (...) { continue; }

    m.user = unescape_tsv(cols[1]);
    const std::string& payload = cols[2];

    if (is_legacy_gcm_line(payload)) {
      // старые зашифрованные записи — пропускаем
      continue;
    } else if (payload.rfind("BLOB:", 0) == 0) {
      if (!enc_enabled_) continue;
      std::vector<uint8_t> blob;
      if (!parse_blob_hex(payload, blob)) continue;
      try {
        const std::string secret(enc_key_.begin(), enc_key_.end());
        auto rec = crypto::decrypt(secret, blob);
        m.text.assign(rec.begin(), rec.end());
      } catch (...) {
        continue;
      }
    } else {
      m.text = unescape_tsv(payload);
    }

    m.hash_hex = cols[3];
    users_out.insert(m.user);

    std::lock_guard<std::mutex> lk(mx_);
    if (ring_.size() >= cap_) ring_.erase(ring_.begin());
    ring_.push_back(std::move(m));
  }
  return true;
}

void Storage::append(const Message& m) {
  {
    std::lock_guard<std::mutex> lk(mx_);
    if (ring_.size() >= cap_) ring_.erase(ring_.begin());
    ring_.push_back(m);
  }

  if (!log_.is_open()) return;

  if (enc_enabled_) {
    try {
      const std::string secret(enc_key_.begin(), enc_key_.end());
      crypto::EncryptedBlob b = crypto::encrypt(
        secret,
        std::vector<uint8_t>(m.text.begin(), m.text.end())
      );
      const std::string blob_hex = hex_encode(b.data);
      log_ << m.ts_ms << '\t'
           << escape_tsv(m.user) << '\t'
           << "BLOB:" << blob_hex << '\t'
           << m.hash_hex << '\n';
      log_.flush();
      return;
    } catch (...) {
      // fallback ниже
    }
  }

  // plaintext fallback
  log_ << m.ts_ms << '\t'
       << escape_tsv(m.user) << '\t'
       << escape_tsv(m.text) << '\t'
       << m.hash_hex << '\n';
  log_.flush();
}

std::vector<Message> Storage::last(std::size_t n) {
  std::lock_guard<std::mutex> lk(mx_);
  if (n >= ring_.size()) return ring_;
  return std::vector<Message>(ring_.end()-n, ring_.end());
}

} // namespace lanchat
