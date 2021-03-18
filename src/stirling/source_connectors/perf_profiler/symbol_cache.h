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

  void Flush();

  int64_t stat_accesses() { return stat_accesses_; }
  int64_t stat_hits() { return stat_hits_; }

  // BCC's symbol resolver assumes the kernel PID is -1.
  static constexpr int kKernelPID = -1;

 private:
  const int pid_;
  absl::flat_hash_map<uintptr_t, std::string> sym_cache_;

  int64_t stat_accesses_ = 0;
  int64_t stat_hits_ = 0;
};

}  // namespace stirling
}  // namespace pl
