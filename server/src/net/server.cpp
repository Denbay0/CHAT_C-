#include "net/server.hpp"
#include "net/protocol.hpp"
#include "hash/hash.hpp"
#include "util/utils.hpp"
#include "storage/storage.hpp"
#include "config/config.hpp"

#include <iostream>
#include <algorithm>
#include <cstring>
#include <csignal>
#include <filesystem>

#ifdef _WIN32
  #include <windows.h>
#endif

namespace lanchat {

Server::Server(const Config& cfg)
  : cfg_(cfg), storage_(/*last_cap*/200) {}

Server::~Server(){ stop(); }

bool Server::start(){
#ifdef _WIN32
  WSADATA wsa; if (WSAStartup(MAKEWORD(2,2), &wsa)!=0){ std::cerr<<"WSAStartup failed\n"; return false; }
#endif

  // открыть storage и подготовить users.log
  if (!storage_.open(cfg_.data_dir)){
    std::cerr<<"Cannot open data dir/log\n";
    return false;
  }

  // включить шифрование логов, если задан ключ
  if (cfg_.enc_enabled){
    std::vector<uint8_t> key32;
    if (!hex_decode(cfg_.enc_key_hex, key32) || key32.size()!=32){
      std::cerr<<"Invalid --enc-key-hex (must be 64 hex chars)\n";
      return false;
    }
    storage_.enable_encryption(key32);
  }

  {
    std::filesystem::create_directories(cfg_.data_dir);
    std::filesystem::path up = std::filesystem::path(cfg_.data_dir) / "users.log";
    users_log_.open(up.string(), std::ios::app);
  }

  // подгрузить историю и пользователей из messages.log
  storage_.load_from_log(/*max_lines*/2000, users_);

  // сетап сокета
  srv_ = socket(AF_INET, SOCK_STREAM, 0);
  if (srv_ == INVALID_SOCK){ std::cerr<<"socket() failed\n"; return false; }

  int yes=1;
  setsockopt(srv_, SOL_SOCKET, SO_REUSEADDR, (char*)&yes, sizeof(yes));

  sockaddr_in addr{}; addr.sin_family = AF_INET; addr.sin_port = htons(cfg_.port);
  if (inet_pton(AF_INET, cfg_.bind_addr.c_str(), &addr.sin_addr) != 1){
    std::cerr<<"Bad bind address\n"; return false;
  }
  if (bind(srv_, (sockaddr*)&addr, sizeof(addr)) == SOCK_ERROR){
    std::cerr<<"bind() failed: "<<GET_LAST_SOCK_ERR<<"\n"; return false;
  }
  if (listen(srv_, 16) == SOCK_ERROR){
    std::cerr<<"listen() failed\n"; return false;
  }

#ifndef _WIN32
  std::signal(SIGINT, [](int){ /* noop */ });
  std::signal(SIGTERM, [](int){ /* noop */ });
#endif

  std::thread([this]{ accept_loop(); }).detach();
  std::cout<<"Server listening on "<<cfg_.bind_addr<<":"<<cfg_.port
           <<" | data="<<cfg_.data_dir
           <<(cfg_.enc_enabled ? " | log-encryption=AES-GCM" : "") << "\n";
  return true;
}

void Server::stop(){
  if (stop_.exchange(true)) return;
  if (srv_!=INVALID_SOCK){ CLOSESOCK(srv_); srv_=INVALID_SOCK; }
#ifdef _WIN32
  WSACleanup();
#endif
}

void Server::accept_loop(){
  while(!stop_.load()){
    sockaddr_in caddr{}; socklen_t clen=sizeof(caddr);
    socket_t cs = accept(srv_, (sockaddr*)&caddr, &clen);
    if (cs==INVALID_SOCK){
      if (stop_.load()) break;
      continue;
    }
    auto cli = std::make_shared<ClientConn>();
    cli->sock = cs;
    {
      std::lock_guard<std::mutex> lk(clients_mx_);
      clients_.push_back(cli);
    }
    std::thread(client_thread, this, cli).detach();
  }
}

bool Server::send_history(socket_t s){
  auto snapshot = storage_.last(cfg_.history_on_join);
  for (const auto& m : snapshot){
    std::string p = make_broadcast(m.ts_ms, m.user, m.text);
    if (!send_frame(s, MSG_BROADCAST, p)) return false;
  }
  return true;
}

void Server::client_thread(Server* self, std::shared_ptr<ClientConn> cli){
  // HELLO
  uint8_t hdr[5];
  if (!read_exact(cli->sock, hdr, 5)) goto done;
  if (hdr[0] != HELLO){ send_error(cli->sock, "Expected HELLO"); goto done; }
  {
    uint32_t len = from_be32(*(uint32_t*)(hdr+1));
    if (len==0 || len>1024){ send_error(cli->sock, "Bad HELLO"); goto done; }
    std::string username(len, '\0');
    if (!read_exact(cli->sock, username.data(), len)) goto done;
    username.erase(std::remove_if(username.begin(), username.end(),
                   [](unsigned char c){ return c=='\r'||c=='\n'; }), username.end());
    if (username.empty()){ send_error(cli->sock, "Empty username"); goto done; }
    cli->username = username;

    // учтём пользователя и при необходимости допишем в users.log
    {
      bool need_write = false;
      if (self->users_.insert(cli->username).second) need_write = true;
      if (need_write && self->users_log_.is_open()){
        self->users_log_ << cli->username << "\n";
        self->users_log_.flush();
      }
    }
  }
  if (!send_ok(cli->sock)) goto done;

  // история
  if (!self->send_history(cli->sock)) goto done;

  // цикл чтения сообщений
  while(!self->stop_.load()){
    if (!read_exact(cli->sock, hdr, 5)) break;
    uint8_t type = hdr[0];
    uint32_t plen = from_be32(*(uint32_t*)(hdr+1));
    if (plen > (1u<<20)){ send_error(cli->sock, "Payload too big"); break; }
    std::string payload(plen, '\0');
    if (plen && !read_exact(cli->sock, payload.data(), plen)) break;

    if (type == MSG){
      self->on_message(cli, payload);
    } // остальное игнорируем
  }

done:
  cli->alive = false;
  CLOSESOCK(cli->sock);
  {
    std::lock_guard<std::mutex> lk(self->clients_mx_);
    self->clients_.erase(std::remove_if(self->clients_.begin(), self->clients_.end(),
      [&](const std::shared_ptr<ClientConn>& c){ return c.get()==cli.get(); }), self->clients_.end());
  }
}

void Server::on_message(const std::shared_ptr<ClientConn>& cli, const std::string& text){
  Message m;
  m.ts_ms = now_ms();
  m.user  = cli->username;
  m.text  = text;

  std::string sig = std::to_string(m.ts_ms) + "|" + m.user + "|" + m.text + "|" + cfg_.secret;
  m.hash_hex = hex64(fnv1a64(sig));

  storage_.append(m);

  std::string payload = make_broadcast(m.ts_ms, m.user, m.text);
  std::lock_guard<std::mutex> lk(clients_mx_);
  for (auto it = clients_.begin(); it != clients_.end(); ){
    auto c = *it;
    if (!c->alive.load()){
      it = clients_.erase(it);
      continue;
    }
    if (!send_frame(c->sock, MSG_BROADCAST, payload)){
      c->alive = false;
      CLOSESOCK(c->sock);
      it = clients_.erase(it);
    } else {
      ++it;
    }
  }
}

} // namespace lanchat
