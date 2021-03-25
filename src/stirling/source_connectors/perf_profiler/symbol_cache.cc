#include "src/stirling/source_connectors/perf_profiler/symbol_cache.h"

DEFINE_bool(stirling_profiler_symcache, true, "Enable the Stirling managed symbol cache.");

namespace pl {
namespace stirling {

namespace {
const std::string& SymbolOrAddr(ebpf::BPFStackTable* stack_traces, const uintptr_t addr,
                                const int pid) {
  static constexpr std::string_view kUnknown = "[UNKNOWN]";
  static std::string sym_or_addr;
  sym_or_addr = stack_traces->get_addr_symbol(addr, pid);
  if (sym_or_addr == kUnknown) {
    sym_or_addr = absl::StrFormat("0x%016llx", addr);
  }
  return sym_or_addr;
}
}  // namespace

const std::string& SymbolCache::LookupSym(ebpf::BPFStackTable* stack_traces, const uintptr_t addr) {
  if (!FLAGS_stirling_profiler_symcache) {
    return SymbolOrAddr(stack_traces, addr, pid_);
  }

  ++stat_accesses_;

  auto sym_iter = sym_cache_.find(addr);
  if (sym_iter == sym_cache_.end()) {
    sym_iter = sym_cache_.try_emplace(addr, SymbolOrAddr(stack_traces, addr, pid_)).first;
  } else {
    ++stat_hits_;
  }
  return sym_iter->second;
}

void SymbolCache::Flush() { sym_cache_.clear(); }

}  // namespace stirling
}  // namespace pl
