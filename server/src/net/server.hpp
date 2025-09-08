#ifndef LANCHAT_NET_SERVER_HPP
#define LANCHAT_NET_SERVER_HPP

#include "storage/storage.hpp"
#include "config/config.hpp"
#include "util/utils.hpp"

#include <unordered_set>
#include <fstream>
#include <vector>
#include <memory>
#include <string>
#include <atomic>
#include <mutex>
#include <thread>

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

  std::unordered_set<std::string> users_;
  std::ofstream users_log_;
};

}

#endif