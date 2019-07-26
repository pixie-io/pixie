#pragma once

#include <bcc/BPF.h>

extern "C" {
#include "src/stirling/bcc_bpf/log_event.h"
}
#include "src/common/base/status.h"

DECLARE_bool(enable_bpf_logging);

// TODO(oazizi): Should this be part of BCCConnector? Or to make it work closer with BCCConnector?
// For example, it seems we should expose ProbeSpec instead of providing APIs to work with BPF
// object directly.

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
