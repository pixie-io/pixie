#ifdef __linux__

#include "src/stirling/bpf_tools/bpftrace_wrapper.h"

#include "src/common/testing/testing.h"

namespace px {
namespace stirling {
namespace bpf_tools {

using ::px::testing::status::StatusIs;
using ::testing::HasSubstr;

TEST(BPFTracerWrapperTest, MapRead) {
  constexpr std::string_view kScript = R"(
  interval:ms:100 {
      @retval[0] = nsecs;
  }
  )";

  BPFTraceWrapper bpftrace_wrapper;
  ASSERT_OK(bpftrace_wrapper.CompileForMapOutput(kScript, /* params */ {}));
  ASSERT_OK(bpftrace_wrapper.Deploy());
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
  ASSERT_OK(bpftrace_wrapper.CompileForMapOutput(kScript, /* params */ {}));
  ASSERT_OK(bpftrace_wrapper.Deploy());
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
  ASSERT_OK(bpftrace_wrapper.CompileForPrintfOutput(script, /* params */ {}));
  ASSERT_OK(bpftrace_wrapper.Deploy(callback_fn));
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
            printf("%llu %u %s %s\n", nsecs, pid, comm, ntop(0));
        }
    )";

  BPFTraceWrapper bpftrace_wrapper;
  ASSERT_OK(bpftrace_wrapper.CompileForPrintfOutput(script, /* params */ {}));

  const std::vector<bpftrace::Field>& fields = bpftrace_wrapper.OutputFields();

  ASSERT_EQ(fields.size(), 4);

  EXPECT_EQ(fields[0].type.type, bpftrace::Type::integer);
  EXPECT_EQ(fields[0].type.size, 8);

  EXPECT_EQ(fields[1].type.type, bpftrace::Type::integer);
  EXPECT_EQ(fields[1].type.size, 8);

  EXPECT_EQ(fields[2].type.type, bpftrace::Type::string);
  EXPECT_EQ(fields[2].type.size, 16);

  EXPECT_EQ(fields[3].type.type, bpftrace::Type::inet);
  EXPECT_EQ(fields[3].type.size, 24);
}

TEST(BPFTracerWrapperTest, MultiplePrintfs) {
  std::string script = R"(
          interval:ms:100 {
              printf("time_:%llu id:%u s:%s ip:%s", nsecs, pid, comm, ntop(0));
              printf("time_:%llu id:%u s:%s ip:%s", nsecs, tid, "foo", ntop(1));
          }
      )";

  BPFTraceWrapper bpftrace_wrapper;
  ASSERT_OK(bpftrace_wrapper.CompileForPrintfOutput(script, /* params */ {}));
  const std::vector<bpftrace::Field>& fields = bpftrace_wrapper.OutputFields();

  ASSERT_EQ(fields.size(), 4);

  EXPECT_EQ(fields[0].type.type, bpftrace::Type::integer);
  EXPECT_EQ(fields[0].type.size, 8);

  EXPECT_EQ(fields[1].type.type, bpftrace::Type::integer);
  EXPECT_EQ(fields[1].type.size, 8);

  EXPECT_EQ(fields[2].type.type, bpftrace::Type::string);
  EXPECT_EQ(fields[2].type.size, 16);

  EXPECT_EQ(fields[3].type.type, bpftrace::Type::inet);
  EXPECT_EQ(fields[3].type.size, 24);
}

TEST(BPFTracerWrapperTest, InconsistentPrintfs) {
  {
    std::string script = R"(
            interval:ms:100 {
                printf("time_:%llu id:%u s:%s ip:%s", nsecs, pid, comm, ntop(0));
                printf("time_:%llu XX:%u s:%s ip:%s", nsecs, tid, "foo", ntop(1));
            }
        )";

    BPFTraceWrapper bpftrace_wrapper;
    ASSERT_THAT(
        bpftrace_wrapper.CompileForPrintfOutput(script, /* params */ {}).status(),
        StatusIs(statuspb::INTERNAL,
                 HasSubstr("All printf statements must have exactly the same format string")));
  }

  {
    std::string script = R"(
            interval:ms:100 {
                printf("time_:%llu id:%u s:%s ip:%s", nsecs, pid, comm, ntop(0));
                printf("time_:%llu id:%llu s:%s ip:%s", nsecs, tid, "foo", ntop(1));
            }
        )";

    BPFTraceWrapper bpftrace_wrapper;
    ASSERT_THAT(
        bpftrace_wrapper.CompileForPrintfOutput(script, /* params */ {}).status(),
        StatusIs(statuspb::INTERNAL,
                 HasSubstr("All printf statements must have exactly the same format string")));
  }
}

}  // namespace bpf_tools
}  // namespace stirling
}  // namespace px

#endif
