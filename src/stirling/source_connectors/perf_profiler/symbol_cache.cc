#include "src/stirling/source_connectors/perf_profiler/symbol_cache.h"

DEFINE_bool(stirling_profiler_symcache, false, "Enable the Stirling managed symbol cache.");

namespace pl {
namespace stirling {

const std::string& SymbolCache::LookupSym(ebpf::BPFStackTable* stack_traces, uintptr_t addr) {
  if (!FLAGS_stirling_profiler_symcache) {
    static std::string value;
    value = stack_traces->get_addr_symbol(addr, pid_);
    return value;
  }

  auto sym_iter = sym_cache_.find(addr);
  if (sym_iter == sym_cache_.end()) {
    sym_iter = sym_cache_.try_emplace(addr, stack_traces->get_addr_symbol(addr, pid_)).first;
  }
  return sym_iter->second;
}

}  // namespace stirling
}  // namespace pl
