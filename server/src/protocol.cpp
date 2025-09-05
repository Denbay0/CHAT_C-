#include "protocol.hpp"
#include <cstring>

namespace lanchat {

bool send_frame(socket_t s, uint8_t type, const std::string& payload){
  uint8_t hdr[5];
  hdr[0] = type;
  uint32_t len_be = to_be32((uint32_t)payload.size());
  std::memcpy(hdr+1, &len_be, 4);
  if (!write_exact(s, hdr, 5)) return false;
  if (!payload.empty() && !write_exact(s, payload.data(), payload.size())) return false;
  return true;
}

bool send_ok(socket_t s){ return send_frame(s, OK, ""); }
bool send_error(socket_t s, const std::string& err){ return send_frame(s, ERR, err); }

std::string make_broadcast(uint64_t ts_ms, const std::string& user, const std::string& text){
  const uint16_t ulen = (uint16_t)(user.size() > 65535 ? 65535 : user.size());
  const uint32_t mlen = (uint32_t)text.size();
  std::string payload;
  payload.resize(8 + 2 + ulen + 4 + mlen);
  size_t off=0;
  uint64_t ts_be = to_be64(ts_ms);
  uint16_t u_be  = to_be16(ulen);
  uint32_t m_be  = to_be32(mlen);
  std::memcpy(&payload[off], &ts_be, 8); off+=8;
  std::memcpy(&payload[off], &u_be, 2);  off+=2;
  std::memcpy(&payload[off], user.data(), ulen); off+=ulen;
  std::memcpy(&payload[off], &m_be, 4); off+=4;
  if (mlen) std::memcpy(&payload[off], text.data(), mlen);
  return payload;
}

} // namespace lanchat
