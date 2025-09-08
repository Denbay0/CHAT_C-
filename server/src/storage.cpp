#include "storage.hpp"
#include "utils.hpp"
#include "crypto.hpp"
#include <filesystem>
#include <sstream>
#include <fstream>

namespace lanchat {

// "GCM:iv_hex:tag_hex:ct_hex"
static bool parse_gcm_line(const std::string& s,
                           std::vector<uint8_t>& iv,
                           std::vector<uint8_t>& tag,
                           std::vector<uint8_t>& ct)
{
  if (s.rfind("GCM:", 0) != 0) return false;
  size_t p1 = s.find(':', 4);
  if (p1 == std::string::npos) return false;
  size_t p2 = s.find(':', p1 + 1);
  if (p2 == std::string::npos) return false;

  std::string ivh  = s.substr(4, p1 - 4);
  std::string tagh = s.substr(p1 + 1, p2 - (p1 + 1));
  std::string cth  = s.substr(p2 + 1);
  return hex_decode(ivh, iv) && hex_decode(tagh, tag) && hex_decode(cth, ct);
}

Storage::Storage(std::size_t last_cap) : cap_(last_cap) {}

bool Storage::open(const std::string& data_dir){
  data_dir_ = data_dir;
  std::filesystem::create_directories(data_dir_);
  std::filesystem::path p = std::filesystem::path(data_dir_) / "messages.log";
  log_.open(p.string(), std::ios::app);
  return log_.is_open();
}

bool Storage::load_from_log(std::size_t max_lines, std::unordered_set<std::string>& users_out){
  std::filesystem::path p = std::filesystem::path(data_dir_) / "messages.log";
  if (!std::filesystem::exists(p)) return true;

  // читаем файл и держим только последние max_lines
  std::vector<std::string> lines;
  {
    std::ifstream in(p.string());
    if (!in.is_open()) return false;
    std::string line;
    while (std::getline(in, line)){
      lines.push_back(std::move(line));
      if (lines.size() > max_lines) lines.erase(lines.begin());
    }
  }

  // парсим и (если можем) расшифровываем
  for (const auto& line : lines){
    // ожидаем 4 колонки: ts \t user \t textOrGCM \t hash
    std::vector<std::string> cols; cols.reserve(4);
    std::string cur; cur.reserve(line.size());
    for (size_t i=0; i<line.size(); ++i){
      char c = line[i];
      if (c=='\t'){ cols.push_back(cur); cur.clear(); }
      else cur.push_back(c);
    }
    cols.push_back(cur);
    if (cols.size() < 4) continue;

    Message m{};
    try {
      m.ts_ms = static_cast<uint64_t>(std::stoull(cols[0]));
    } catch(...) { continue; }

    m.user = unescape_tsv(cols[1]);

    if (cols[2].rfind("GCM:", 0) == 0){
      // зашифровано — пробуем расшифровать, если есть ключ
      if (!enc_enabled_ || enc_key_.size()!=32){
        // нет ключа — пропустим такие записи (чтобы не засорять память непонятным текстом)
        continue;
      }
      std::vector<uint8_t> iv, tag, ct;
      if (!parse_gcm_line(cols[2], iv, tag, ct)) continue;
      GcmBlob blob{ std::move(iv), std::move(tag), std::move(ct) };
      try{
        auto plain = aes_gcm_encrypt_win; // чтобы компилятор не ругался на неиспользуемые инклуды
        (void)plain;
        auto rec = aes_gcm_decrypt_win(enc_key_, blob);
        m.text.assign(rec.begin(), rec.end());
      }catch(...){
        continue; // неверный ключ/повреждение — пропускаем
      }
    } else {
      // обычный текст
      m.text = unescape_tsv(cols[2]);
    }

    m.hash_hex = cols[3];
    users_out.insert(m.user);

    // в кольцевой буфер
    {
      std::lock_guard<std::mutex> lk(mx_);
      if (ring_.size() >= cap_) ring_.erase(ring_.begin());
      ring_.push_back(std::move(m));
    }
  }
  return true;
}

void Storage::append(const Message& m){
  // в память
  {
    std::lock_guard<std::mutex> lk(mx_);
    if (ring_.size() >= cap_) ring_.erase(ring_.begin());
    ring_.push_back(m);
  }

  // в файл: если есть ключ — шифруем поле text
  if (!log_.is_open()) return;

  if (enc_enabled_ && enc_key_.size()==32){
    try{
      GcmBlob b = aes_gcm_encrypt_win(enc_key_,
                                      std::vector<uint8_t>(m.text.begin(), m.text.end()));
      std::string gcm = "GCM:" + hex_encode(b.iv) + ":" + hex_encode(b.tag) + ":" + hex_encode(b.ct);
      log_ << m.ts_ms << '\t' << escape_tsv(m.user) << '\t' << gcm << '\t' << m.hash_hex << '\n';
      log_.flush();
      return;
    }catch(...){
      // если шифрование не удалось — пишем как plaintext (fallback)
    }
  }

  log_ << m.ts_ms << '\t'
       << escape_tsv(m.user) << '\t'
       << escape_tsv(m.text) << '\t'
       << m.hash_hex << '\n';
  log_.flush();
}

std::vector<Message> Storage::last(std::size_t n){
  std::lock_guard<std::mutex> lk(mx_);
  if (n >= ring_.size()) return ring_;
  return std::vector<Message>(ring_.end()-n, ring_.end());
}

} // namespace lanchat
