#ifdef __linux__
#include <cstring>
#include <ctime>

#include "src/common/system_config/system_config.h"
#include "src/stirling/source_connector.h"

namespace pl {
namespace stirling {

// Utility function to convert time as recorded by bpftrace through the 'nsecs' built-in to
// real-time. BPF provides only access to CLOCK_MONOTONIC values (through nsecs), so have to
// determine the offset.
void SourceConnector::InitClockRealTimeOffset() {
  // TODO(oazizi): We don't need to go call InitClockRealTimeOffset() anymore. Get rid of it and
  // update use of ClockRealTimeOffset().
  auto sysconfig = common::SystemConfig::GetInstance();
  real_time_offset_ = sysconfig->ClockRealTimeOffset();
}

uint64_t SourceConnector::ClockRealTimeOffset() { return real_time_offset_; }

}  // namespace stirling
}  // namespace pl

#endif
