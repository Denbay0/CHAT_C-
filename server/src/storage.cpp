#include "storage.hpp"
#include "utils.hpp"
#include <filesystem>

namespace lanchat {

Storage::Storage(std::size_t last_cap) : cap_(last_cap) {}
Storage::~Storage(){ if (log_.is_open()) log_.close(); }

bool Storage::open(const std::string& data_dir){
  std::filesystem::create_directories(data_dir);
  std::filesystem::path p = std::filesystem::path(data_dir) / "messages.log";
  log_.open(p.string(), std::ios::app);
  return log_.is_open();
}

void Storage::append(const Message& m){
  {
    std::lock_guard<std::mutex> lk(mx_);
    if (ring_.size() >= cap_) ring_.erase(ring_.begin());
    ring_.push_back(m);
  }
  {
    std::lock_guard<std::mutex> lk(mx_);
    if (log_.is_open()){
      log_ << m.ts_ms << '\t'
           << escape_tsv(m.user) << '\t'
           << escape_tsv(m.text) << '\t'
           << m.hash_hex << '\n';
      log_.flush();
    }
  }
}

std::vector<Message> Storage::last(std::size_t n){
  std::lock_guard<std::mutex> lk(mx_);
  if (n >= ring_.size()) return ring_;
  return std::vector<Message>(ring_.end()-n, ring_.end());
}

} // namespace lanchat
