#ifdef __linux__
#include <cstring>
#include <ctime>

#include "src/stirling/source_connector.h"

namespace pl {
namespace stirling {

// Utility function to convert time as recorded by bpftrace through the 'nsecs' built-in to
// real-time. BPF provides only access to CLOCK_MONOTONIC values (through nsecs), so have to
// determine the offset.
void SourceConnector::InitClockRealTimeOffset() {
  struct timespec time, real_time;
  clock_gettime(CLOCK_MONOTONIC, &time);
  clock_gettime(CLOCK_REALTIME, &real_time);

  real_time_offset_ =
      kSecToNanosecFactor * (real_time.tv_sec - time.tv_sec) + real_time.tv_nsec - time.tv_nsec;
}

uint64_t SourceConnector::ClockRealTimeOffset() { return real_time_offset_; }

}  // namespace stirling
}  // namespace pl

#endif
