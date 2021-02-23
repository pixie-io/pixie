#include "src/stirling/source_connectors/perf_profiler/symbolizer.h"

#include <string>
#include <vector>

#include <absl/strings/str_cat.h>
#include "src/common/base/base.h"

#include "src/stirling/source_connectors/perf_profiler/bcc_bpf_intf/stack_event.h"

namespace pl {
namespace stirling {
namespace stack_traces {

std::string FoldedStackTraceString(const std::vector<std::string>& user_symbols,
                                   const std::vector<std::string>& kernel_symbols) {
  constexpr char kSeparator = ';';

  std::string out;

  for (const auto& user_symbol : user_symbols) {
    absl::StrAppend(&out, user_symbol);
    out += kSeparator;
  }

  // Note that kernel symbols are added in reverse order. This is how BCC does it,
  // so apparently  kernel symbols are populated in reverse order by BPF/BCC.
  for (auto iter = kernel_symbols.rbegin(); iter != kernel_symbols.rend(); ++iter) {
    // Add "_[k]" suffix to all the symbols in the kernel stack trace:
    absl::StrAppend(&out, *iter, "_[k]");
    out += kSeparator;
  }

  // Strip off final separator.
  if (!out.empty()) {
    DCHECK_EQ(out.back(), kSeparator);
    out.resize(out.size() - 1);
  }

  return out;
}

}  // namespace stack_traces
}  // namespace stirling
}  // namespace pl
