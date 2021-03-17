#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <string>

#include "src/stirling/source_connectors/perf_profiler/symbolizer.h"

namespace pl {
namespace stirling {
namespace stack_traces {

TEST(Symbolizer, Basic) {
  std::string_view name = "top";
  std::vector<std::string> symbols = {"syscall_helper_[k]", "syscall_[k]", "Foo()", "Run()"};

  std::string folded_stack_trace = FoldedStackTraceString(name, symbols);

  EXPECT_EQ(folded_stack_trace, "top;Run();Foo();syscall_[k];syscall_helper_[k]");
}

}  // namespace stack_traces
}  // namespace stirling
}  // namespace pl
