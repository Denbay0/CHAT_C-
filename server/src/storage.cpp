#include "storage.hpp"

namespace lanchat {

Storage::Storage(std::size_t last_cap) : cap_(last_cap) {}

std::vector<Message> Storage::last(std::size_t n){
  std::lock_guard<std::mutex> lk(mx_);
  if (n >= ring_.size()) return ring_;
  return std::vector<Message>(ring_.end()-n, ring_.end());
}

void Storage::append(const Message& m){
  std::lock_guard<std::mutex> lk(mx_);
  if (ring_.size() >= cap_) ring_.erase(ring_.begin());
  ring_.push_back(m);
}

} // namespace lanchat
