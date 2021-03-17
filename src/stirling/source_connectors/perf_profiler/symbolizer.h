#pragma once

#include <string>
#include <vector>

#include "src/stirling/source_connectors/perf_profiler/bcc_bpf_intf/stack_event.h"

namespace pl {
namespace stirling {
namespace stack_traces {

/**
 * Formats a stack trace in folded format, for consumption by flame graph tools.
 * Simply a list of symbols separated by semicolon.
 * Example:
 * pem;[unknown];pl::stirling::StirlingImpl::RunCore();pl::stirling::InfoClassManager::SampleData(pl::stirling::ConnectorContext*)
 */
std::string FoldedStackTraceString(std::string_view binary_name,
                                   const std::vector<std::string>& symbols);

}  // namespace stack_traces
}  // namespace stirling
}  // namespace pl
