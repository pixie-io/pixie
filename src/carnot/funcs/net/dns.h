/*
 * Copyright 2018- The Pixie Authors.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <string>

#define TBB_PREVIEW_CONCURRENT_LRU_CACHE 1
#include "tbb/concurrent_lru_cache.h"

namespace px {
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
}  // namespace px
