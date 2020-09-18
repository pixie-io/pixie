#ifdef __linux__

#include "src/stirling/bpf_tools/bpftrace_wrapper.h"

#include "src/common/testing/testing.h"

namespace pl {
namespace stirling {
namespace bpf_tools {

TEST(BPFTracerWrapperTest, MapRead) {
  constexpr std::string_view kScript = R"(
  interval:ms:100 {
      @retval[0] = nsecs;
  }
  )";

  BPFTraceWrapper bpftrace_wrapper;
  ASSERT_OK(bpftrace_wrapper.Deploy(kScript, /* params */ {}));
  sleep(1);

  bpftrace::BPFTraceMap entries = bpftrace_wrapper.GetBPFMap("@retval");
  EXPECT_FALSE(entries.empty());

  bpftrace_wrapper.Stop();
}

TEST(BPFTracerWrapperTest, PerfBufferPoll) {
  constexpr std::string_view kScript = R"(
    interval:ms:100 {
        printf("%llu\n", nsecs);
    }
    )";

  BPFTraceWrapper bpftrace_wrapper;
  ASSERT_OK(bpftrace_wrapper.Deploy(kScript, /* params */ {}));
  sleep(1);

  bpftrace_wrapper.PollPerfBuffers(100);

  bpftrace_wrapper.Stop();
}

// To show that the callback can be to a member function.
class CallbackWrapperClass {
 public:
  void PrintfCallback(uint8_t* /* data */) { ++callback_count; }

  int callback_count = 0;
};

TEST(BPFTracerWrapperTest, PerfBufferPollWithCallback) {
  constexpr int kProbeDurationSeconds = 1;
  constexpr int kProbeIntervalMilliseconds = 100;

  std::string script = R"(
      interval:ms:$0 {
          printf("%llu\n", nsecs);
      }
  )";
  script = absl::Substitute(script, kProbeIntervalMilliseconds);

  CallbackWrapperClass callback_target;

  BPFTraceWrapper bpftrace_wrapper;
  auto callback_fn =
      std::bind(&CallbackWrapperClass::PrintfCallback, &callback_target, std::placeholders::_1);
  ASSERT_OK(bpftrace_wrapper.Deploy(script, /* params */ {}, callback_fn));
  sleep(kProbeDurationSeconds);

  bpftrace_wrapper.PollPerfBuffers();

  // The callback should be called `Duration / ProbeInterval` times,
  // But give a much broader range because sleep() is not precise and we don't want a flaky test.

  constexpr int kExpectedCalls = (kProbeDurationSeconds * 1000) / kProbeIntervalMilliseconds;
  constexpr int kMargin = 5;
  EXPECT_GE(callback_target.callback_count, kExpectedCalls - kMargin);
  EXPECT_LE(callback_target.callback_count, kExpectedCalls + kMargin);

  bpftrace_wrapper.Stop();
}

TEST(BPFTracerWrapperTest, OutputFields) {
  std::string script = R"(
        interval:ms:100 {
            printf("%llu %u %s\n", nsecs, pid, comm);
        }
    )";

  BPFTraceWrapper bpftrace_wrapper;
  ASSERT_OK(bpftrace_wrapper.Deploy(script, /* params */ {}));
  sleep(1);

  ASSERT_OK_AND_ASSIGN(const std::vector<bpftrace::Field>& fields, bpftrace_wrapper.OutputFields());
  EXPECT_EQ(fields.size(), 3);
  EXPECT_EQ(fields[0].type.type, bpftrace::Type::integer);
  EXPECT_EQ(fields[0].type.size, 8);

  EXPECT_EQ(fields[1].type.type, bpftrace::Type::integer);
  EXPECT_EQ(fields[1].type.size, 8);

  EXPECT_EQ(fields[2].type.type, bpftrace::Type::string);
  EXPECT_EQ(fields[2].type.size, 16);

  bpftrace_wrapper.Stop();
}

}  // namespace bpf_tools
}  // namespace stirling
}  // namespace pl

#endif
