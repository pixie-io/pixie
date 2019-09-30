#include <bcc/BPF.h>
#include <gtest/gtest.h>

#include "absl/strings/str_cat.h"
#include "src/stirling/pid_runtime_connector.h"
#include "src/stirling/socket_trace_connector.h"

namespace pl {
namespace stirling {

struct TestParam {
  std::string_view connector;
  std::string_view bpf;
};

// This only checks BPF C code's syntax. No verification or actually execution inside BPF runtime.
//
// TODO(PL-538): For now we require people manually add tests for new BPF C files. In the future,
// we plan to wrap pl_cc_resource() into a dedicated genrule for BPF C files, maybe call it
// pl_bpf_c_library(), which automatically generates test that is equivalent to what's being done
// here.
class BPFProgramTest : public ::testing::TestWithParam<TestParam> {
 protected:
  bool LoadBPFProgram(const std::string& c_text) {
    auto init_res = bpf_.init(c_text);
    if (init_res.code() != 0) {
      error_message_ =
          absl::StrCat("Failed to initialize BCC script, error message: ", init_res.msg());
      return false;
    }
    return true;
  }

  ebpf::BPF bpf_;
  std::string error_message_;
};

TEST_P(BPFProgramTest, CheckSyntax) {
  EXPECT_TRUE(LoadBPFProgram(std::string(GetParam().bpf))) << "Error message:\n" << error_message_;
}

INSTANTIATE_TEST_CASE_P(AllConnectors, BPFProgramTest,
                        ::testing::Values(
                            // TODO(yzhao): Remember to add new BPF connectors into this list.
                            TestParam{"SocketTraceConnector", SocketTraceConnector::kBCCScript},
                            TestParam{"PIDRuntimeConnector", PIDRuntimeConnector::kBCCScript}),
                        [](const ::testing::TestParamInfo<TestParam> info) -> std::string {
                          return std::string(info.param.connector);
                        });

}  // namespace stirling
}  // namespace pl
