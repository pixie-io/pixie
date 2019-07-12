#pragma once

#include <bcc/BPF.h>

extern "C" {
#include "src/stirling/bcc_bpf/log_event.h"
}
#include "src/common/base/status.h"

DECLARE_bool(enable_bpf_logging);

namespace pl {
namespace stirling {

/**
 * @brief Initializes perf buffer for logging events from BPF.
 */
Status InitBPFLogging(ebpf::BPF* bpf);

/**
 * @brief Dumps BPF logging events through GLOG logging facility.
 */
void DumpBPFLog(ebpf::BPF* bpf);

}  // namespace stirling
}  // namespace pl
