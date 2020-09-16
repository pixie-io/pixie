#ifdef __linux__

#include "src/stirling/bpf_tools/bpftrace_wrapper.h"

#include "src/common/testing/testing.h"

namespace pl {
namespace stirling {
namespace bpf_tools {

constexpr std::string_view kScript = R"(
interval:ms:100 {
    @retval[0] = nsecs;
}
)";

TEST(BPFTracerWrapperTest, Deploy) {
  BPFTraceWrapper bpftrace_wrapper;
  ASSERT_OK(bpftrace_wrapper.Deploy(kScript, /* params */ {}));
  sleep(1);

  // Shouldn't do anything because this probe doesn't use perf buffers.
  // TODO(oazizi): Update when perf buffers are better supported.
  // bpftrace_wrapper.PollPerfBuffers(100);

  // TODO(oazizi): Enable a map read as well.
  // bpftrace_wrapper.GetBPFMap("retval");

  bpftrace_wrapper.Stop();
}

}  // namespace bpf_tools
}  // namespace stirling
}  // namespace pl

#endif
