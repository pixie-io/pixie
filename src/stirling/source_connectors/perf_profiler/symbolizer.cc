#include "src/stirling/source_connectors/perf_profiler/symbolizer.h"

#include <string>
#include <vector>

#include <absl/strings/str_cat.h>
#include "src/common/base/base.h"

#include "src/stirling/source_connectors/perf_profiler/bcc_bpf_intf/stack_event.h"

namespace pl {
namespace stirling {
namespace stack_traces {

std::string FoldedStackTraceString(std::string_view binary_name,
                                   const std::vector<std::string>& user_symbols,
                                   const std::vector<std::string>& kernel_symbols) {
  constexpr std::string_view kSeparator = ";";
  constexpr std::string_view kKSymSuffix = "_[k]";

  std::string out;

  absl::StrAppend(&out, binary_name, kSeparator);

  // Note that symbols are added in reverse order because of how BCC populates
  // symbols from get_stack_symbol().
  for (auto iter = user_symbols.rbegin(); iter != user_symbols.rend(); ++iter) {
    absl::StrAppend(&out, *iter, kSeparator);
  }

  for (auto iter = kernel_symbols.rbegin(); iter != kernel_symbols.rend(); ++iter) {
    // Add "_[k]" suffix to all the symbols in the kernel stack trace:
    absl::StrAppend(&out, *iter, kKSymSuffix, kSeparator);
  }

  // Strip off final separator.
  out.resize(out.size() - 1);

  return out;
}

}  // namespace stack_traces
}  // namespace stirling
}  // namespace pl
