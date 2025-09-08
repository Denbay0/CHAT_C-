#pragma once
#include <string>
#include "util/utils.hpp"

namespace lanchat {

// кадр: 1 byte type + 4 bytes len(BE) + payload
enum : uint8_t {
  HELLO = 0x01,
  MSG   = 0x02,
  OK    = 0x06,
  ERR   = 0x05,
  MSG_BROADCAST = 0x12
};

bool send_frame(socket_t s, uint8_t type, const std::string& payload);
bool send_ok(socket_t s);
bool send_error(socket_t s, const std::string& err);

// payload MSG_BROADCAST:
// ts_ms(8BE) + ulen(2BE) + username + mlen(4BE) + message
std::string make_broadcast(uint64_t ts_ms,
                           const std::string& user,
                           const std::string& text);

} // namespace lanchat
