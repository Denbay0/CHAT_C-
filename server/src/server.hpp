#pragma once
#include <vector>
#include <memory>
#include <string>
#include <atomic>
#include <mutex>
#include <thread>
#include <unordered_set>
#include <fstream>
#include "utils.hpp"
#include "storage.hpp"
#include "config.hpp"

namespace lanchat {

struct ClientConn {
  socket_t sock;
  std::string username;
  std::atomic<bool> alive{true};
};

class Server {
public:
  explicit Server(const Config& cfg);
  ~Server();

  bool start();
  void stop();

private:
  void accept_loop();
  static void client_thread(Server* self, std::shared_ptr<ClientConn> cli);
  void on_message(const std::shared_ptr<ClientConn>& cli, const std::string& text);
  bool send_history(socket_t s);

private:
  Config cfg_;
  socket_t srv_{INVALID_SOCK};
  std::atomic<bool> stop_{false};

  std::mutex clients_mx_;
  std::vector<std::shared_ptr<ClientConn>> clients_;

  Storage storage_;

  // пользователи в памяти + лог пользователей (опционально)
  std::unordered_set<std::string> users_;
  std::ofstream users_log_;
};

} // namespace lanchat
