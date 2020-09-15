#pragma once

#include <string>

#define TBB_PREVIEW_CONCURRENT_LRU_CACHE 1
#include "tbb/concurrent_lru_cache.h"

namespace pl {
namespace carnot {
namespace funcs {
namespace net {
namespace internal {

constexpr size_t kMaxHostnameSize = 512;
constexpr size_t kLRUCacheSize = 1024;

inline std::string DNSLookup(const std::string& addr) {
  struct sockaddr_in sa;

  char node[kMaxHostnameSize];

  memset(&sa, 0, sizeof sa);
  sa.sin_family = AF_INET;

  inet_pton(AF_INET, addr.c_str(), &sa.sin_addr);

  int res =
      getnameinfo((struct sockaddr*)&sa, sizeof(sa), node, sizeof(node), NULL, 0, NI_NAMEREQD);

  if (res) {
    return res == EAI_NONAME ? addr : gai_strerror(res);
  }
  return node;
}

class DNSCache {
 public:
  static DNSCache& GetInstance() {
    static DNSCache cache;
    return cache;
  }

  std::string Lookup(std::string addr) { return cache_[addr].value(); }

 private:
  DNSCache()
      : cache_([](std::string addr) -> std::string { return DNSLookup(addr); }, kLRUCacheSize) {}

  tbb::concurrent_lru_cache<std::string, std::string> cache_;
};

}  // namespace internal

}  // namespace net
}  // namespace funcs
}  // namespace carnot
}  // namespace pl
