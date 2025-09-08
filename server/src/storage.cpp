#include "storage.hpp"
#include "utils.hpp"
#include <filesystem>
#include <sstream>

namespace lanchat {

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

  // читаем все строки, но держим в памяти только последние max_lines
  std::vector<std::string> lines;
  {
    std::ifstream in(p.string());
    if (!in.is_open()) return false;
    std::string line;
    while (std::getline(in, line)){
      lines.push_back(std::move(line));
      if (lines.size() > max_lines) lines.erase(lines.begin()); // простая "скользящая" выборка
    }
  }

  // распарсим TSV
  for (const auto& line : lines){
    // ожидаем 4 колонки: ts \t user \t text \t hash
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
    m.user    = unescape_tsv(cols[1]);
    m.text    = unescape_tsv(cols[2]);
    m.hash_hex= cols[3];

    users_out.insert(m.user);

    // положим в кольцевой буфер
    {
      std::lock_guard<std::mutex> lk(mx_);
      if (ring_.size() >= cap_) ring_.erase(ring_.begin());
      ring_.push_back(std::move(m));
    }
  }
  return true;
}

void Storage::append(const Message& m){
  {
    std::lock_guard<std::mutex> lk(mx_);
    if (ring_.size() >= cap_) ring_.erase(ring_.begin());
    ring_.push_back(m);
  }
  // в файл
  if (log_.is_open()){
    log_ << m.ts_ms << '\t'
         << escape_tsv(m.user) << '\t'
         << escape_tsv(m.text) << '\t'
         << m.hash_hex << '\n';
    log_.flush();
  }
}

std::vector<Message> Storage::last(std::size_t n){
  std::lock_guard<std::mutex> lk(mx_);
  if (n >= ring_.size()) return ring_;
  return std::vector<Message>(ring_.end()-n, ring_.end());
}

} // namespace lanchat
