#pragma once

#include <string>

#include "src/stirling/bpf_tools/bcc_wrapper.h"

DECLARE_bool(stirling_profiler_symcache);

namespace pl {
namespace stirling {

/**
 * A Stirling managed cache of symbols.
 * While BCC has its own cache as well, it's expensive to use.
 */
class SymbolCache {
 public:
  explicit SymbolCache(int pid) : pid_(pid) {}

  const std::string& LookupSym(ebpf::BPFStackTable* stack_traces, uintptr_t addr);

 private:
  const int pid_;
  absl::flat_hash_map<uintptr_t, std::string> sym_cache_;
};

}  // namespace stirling
}  // namespace pl
