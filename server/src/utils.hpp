#pragma once
#include <cstdint>
#include <chrono>
#include <string>

#ifdef _WIN32
  #include <winsock2.h>
  #include <ws2tcpip.h>
  using socket_t = SOCKET;
  #define CLOSESOCK closesocket
  #define GET_LAST_SOCK_ERR WSAGetLastError()
  #define INVALID_SOCK INVALID_SOCKET
  #define SOCK_ERROR SOCKET_ERROR
#else
  #include <unistd.h>
  #include <sys/socket.h>
  #include <netinet/in.h>
  #include <arpa/inet.h>
  using socket_t = int;
  #define CLOSESOCK ::close
  #define GET_LAST_SOCK_ERR errno
  #define INVALID_SOCK (-1)
  #define SOCK_ERROR (-1)
#endif

namespace lanchat {

inline uint16_t to_be16(uint16_t v){ return htons(v); }
inline uint32_t to_be32(uint32_t v){ return htonl(v); }

inline uint64_t to_be64(uint64_t v){
#if defined(_WIN32)
  uint32_t hi = htonl((uint32_t)(v >> 32));
  uint32_t lo = htonl((uint32_t)(v & 0xFFFFFFFFULL));
  return ((uint64_t)lo << 32) | hi;
#else
  #if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    return ( (uint64_t)htonl((uint32_t)(v & 0xFFFFFFFFULL)) << 32 ) | htonl((uint32_t)(v >> 32));
  #else
    return v;
  #endif
#endif
}
inline uint16_t from_be16(uint16_t v){ return ntohs(v); }
inline uint32_t from_be32(uint32_t v){ return ntohl(v); }
inline uint64_t from_be64(uint64_t v){
#if defined(_WIN32)
  uint32_t lo = ntohl((uint32_t)(v >> 32));
  uint32_t hi = ntohl((uint32_t)(v & 0xFFFFFFFFULL));
  return ((uint64_t)hi << 32) | lo;
#else
  #if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    return ( (uint64_t)ntohl((uint32_t)(v & 0xFFFFFFFFULL)) << 32 ) | ntohl((uint32_t)(v >> 32));
  #else
    return v;
  #endif
#endif
}

inline uint64_t now_ms(){
  using namespace std::chrono;
  return duration_cast<milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
}

inline std::string escape_tsv(const std::string& in){
  std::string out; out.reserve(in.size());
  for(char c: in){
    if (c=='\t') out += "\\t";
    else if (c=='\n') out += "\\n";
    else if (c=='\\') out += "\\\\";
    else out += c;
  }
  return out;
}

inline bool read_exact(socket_t s, void* buf, size_t n){
  char* p = static_cast<char*>(buf);
  size_t got=0;
  while(got<n){
    int r = recv(s, p+got, (int)(n-got), 0);
    if (r==0) return false;
    if (r==SOCK_ERROR) return false;
    got += (size_t)r;
  }
  return true;
}

inline bool write_exact(socket_t s, const void* buf, size_t n){
  const char* p = static_cast<const char*>(buf);
  size_t sent=0;
  while(sent<n){
    int r = send(s, p+sent, (int)(n-sent), 0);
    if (r==SOCK_ERROR) return false;
    sent += (size_t)r;
  }
  return true;
}

} // namespace lanchat
