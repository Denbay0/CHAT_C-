#pragma once
#include <cstdint>
#include <chrono>
#include <string>
#include <vector>
#include <cerrno>   // для errno на *nix

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

// ---------- Byte order helpers (BE <-> host) ----------
inline uint16_t to_be16(uint16_t v){ return htons(v); }
inline uint32_t to_be32(uint32_t v){ return htonl(v); }

inline uint64_t to_be64(uint64_t v){
#if defined(_WIN32)
  uint32_t hi = htonl(static_cast<uint32_t>(v >> 32));
  uint32_t lo = htonl(static_cast<uint32_t>(v & 0xFFFFFFFFULL));
  return (static_cast<uint64_t>(lo) << 32) | hi;
#else
  #if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    return (static_cast<uint64_t>(htonl(static_cast<uint32_t>(v & 0xFFFFFFFFULL))) << 32) |
            htonl(static_cast<uint32_t>(v >> 32));
  #else
    return v;
  #endif
#endif
}

inline uint16_t from_be16(uint16_t v){ return ntohs(v); }
inline uint32_t from_be32(uint32_t v){ return ntohl(v); }

inline uint64_t from_be64(uint64_t v){
#if defined(_WIN32)
  uint32_t lo = ntohl(static_cast<uint32_t>(v >> 32));
  uint32_t hi = ntohl(static_cast<uint32_t>(v & 0xFFFFFFFFULL));
  return (static_cast<uint64_t>(hi) << 32) | lo;
#else
  #if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    return (static_cast<uint64_t>(ntohl(static_cast<uint32_t>(v & 0xFFFFFFFFULL))) << 32) |
            ntohl(static_cast<uint32_t>(v >> 32));
  #else
    return v;
  #endif
#endif
}

// ---------- HEX helpers ----------
inline std::string hex_encode(const std::vector<uint8_t>& v){
  static const char* d = "0123456789abcdef";
  std::string s; s.resize(v.size()*2);
  for (size_t i=0;i<v.size();++i){
    s[2*i]   = d[v[i] >> 4];
    s[2*i+1] = d[v[i] & 0xF];
  }
  return s;
}

inline bool hex_decode(const std::string& hex, std::vector<uint8_t>& out){
  auto hv=[](char c)->int{
    if(c>='0'&&c<='9')return c-'0';
    if(c>='a'&&c<='f')return c-'a'+10;
    if(c>='A'&&c<='F')return c-'A'+10;
    return -1;
  };
  if (hex.size() % 2) return false;
  out.resize(hex.size()/2);
  for (size_t i=0;i<out.size();++i){
    int hi=hv(hex[2*i]), lo=hv(hex[2*i+1]);
    if (hi<0 || lo<0) return false;
    out[i] = static_cast<uint8_t>((hi<<4)|lo);
  }
  return true;
}

// ---------- TSV escape/unescape ----------
inline std::string escape_tsv(const std::string& in){
  std::string out; out.reserve(in.size());
  for (char c: in){
    if (c=='\t') out += "\\t";
    else if (c=='\n') out += "\\n";
    else if (c=='\\') out += "\\\\";
    else out += c;
  }
  return out;
}

inline std::string unescape_tsv(const std::string& in){
  std::string out; out.reserve(in.size());
  for (size_t i=0; i<in.size(); ++i){
    char c = in[i];
    if (c=='\\' && i+1<in.size()){
      char n = in[i+1];
      if (n=='t'){ out.push_back('\t'); ++i; continue; }
      if (n=='n'){ out.push_back('\n'); ++i; continue; }
      if (n=='\\'){ out.push_back('\\'); ++i; continue; }
    }
    out.push_back(c);
  }
  return out;
}

// ---------- Time (ms since epoch) ----------
inline uint64_t now_ms(){
  using namespace std::chrono;
  return duration_cast<milliseconds>(
    std::chrono::system_clock::now().time_since_epoch()).count();
}

// ---------- Socket I/O helpers ----------
inline bool read_exact(socket_t s, void* buf, size_t n){
  char* p = static_cast<char*>(buf);
  size_t got=0;
  while(got<n){
    int r = recv(s, p+got, static_cast<int>(n-got), 0);
    if (r==0) return false;
    if (r==SOCK_ERROR) return false;
    got += static_cast<size_t>(r);
  }
  return true;
}

inline bool write_exact(socket_t s, const void* buf, size_t n){
  const char* p = static_cast<const char*>(buf);
  size_t sent=0;
  while(sent<n){
    int r = send(s, p+sent, static_cast<int>(n-sent), 0);
    if (r==SOCK_ERROR) return false;
    sent += static_cast<size_t>(r);
  }
  return true;
}

} // namespace lanchat
