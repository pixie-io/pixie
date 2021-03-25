#include <gtest/gtest.h>

#include "src/common/testing/testing.h"
#include "src/stirling/bpf_tools/bcc_wrapper.h"
#include "src/stirling/source_connectors/perf_profiler/symbol_cache.h"

namespace pl {
namespace stirling {

TEST(SymbolCacheTest, Basic) {
  bpf_tools::BCCWrapper bcc_wrapper;
  std::string_view kProgram = "BPF_STACK_TRACE(test_stack_traces, 1024);";
  ASSERT_OK(bcc_wrapper.InitBPFProgram(kProgram));
  ebpf::BPFStackTable stack_table = bcc_wrapper.GetStackTable("test_stack_traces");

  FLAGS_stirling_profiler_symcache = true;
  SymbolCache sym_cache(SymbolCache::kKernelPID);

  // Lookup an address for the first time. This should be a cache miss.
  EXPECT_EQ(sym_cache.LookupSym(&stack_table, 0), "0x0000000000000000");
  EXPECT_EQ(sym_cache.stat_accesses(), 1);
  EXPECT_EQ(sym_cache.stat_hits(), 0);

  // Lookup the address a second time. We should get a cache hit.
  EXPECT_EQ(sym_cache.LookupSym(&stack_table, 0), "0x0000000000000000");
  EXPECT_EQ(sym_cache.stat_accesses(), 2);
  EXPECT_EQ(sym_cache.stat_hits(), 1);

  sym_cache.Flush();

  // Lookup the address again, but now we should *not* get a cache hit.
  EXPECT_EQ(sym_cache.LookupSym(&stack_table, 0), "0x0000000000000000");
  EXPECT_EQ(sym_cache.stat_accesses(), 3);
  EXPECT_EQ(sym_cache.stat_hits(), 1);

  // Expect a cache hit:
  EXPECT_EQ(sym_cache.LookupSym(&stack_table, 0), "0x0000000000000000");
  EXPECT_EQ(sym_cache.stat_accesses(), 4);
  EXPECT_EQ(sym_cache.stat_hits(), 2);

  // Turn off caching, expect stats to not change:
  FLAGS_stirling_profiler_symcache = false;
  EXPECT_EQ(sym_cache.LookupSym(&stack_table, 0), "0x0000000000000000");
  EXPECT_EQ(sym_cache.stat_accesses(), 4);
  EXPECT_EQ(sym_cache.stat_hits(), 2);
}

}  // namespace stirling
}  // namespace pl
