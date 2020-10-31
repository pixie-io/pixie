#include <string>

#include "src/common/exec/subprocess.h"
#include "src/common/fs/fs_wrapper.h"
#include "src/common/testing/testing.h"
#include "src/stirling/dynamic_tracing/dynamic_tracer.h"
#include "src/stirling/testing/testing.h"

constexpr std::string_view kBinaryPath =
    "src/stirling/obj_tools/testdata/dummy_go_binary_/dummy_go_binary";

namespace pl {
namespace stirling {
namespace dynamic_tracing {

using ::google::protobuf::TextFormat;
using ::pl::stirling::bpf_tools::UProbeSpec;
using ::pl::stirling::testing::PIDToUPID;
using ::pl::testing::proto::EqualsProto;
using ::pl::testing::status::StatusIs;
using ::testing::EndsWith;
using ::testing::Field;
using ::testing::HasSubstr;
using ::testing::SizeIs;

constexpr char kServerPath[] =
    "src/stirling/protocols/http2/testing/go_grpc_server/go_grpc_server_/go_grpc_server";

constexpr char kPod0UpdateTxt[] = R"(
  uid: "pod0"
  name: "pod0"
  namespace: "ns0"
  start_timestamp_ns: 100
  container_ids: "container0"
  container_ids: "container1"
  container_names: "container0"
  container_names: "container1"
)";

constexpr char kPod1UpdateTxt[] = R"(
  uid: "pod1"
  name: "pod1"
  namespace: "ns0"
  start_timestamp_ns: 100
)";

constexpr char kContainer0UpdateTxt[] = R"(
  cid: "container0"
  name: "container0"
  namespace: "ns0"
  start_timestamp_ns: 100
  pod_id: "pod0"
  pod_name: "pod0"
)";

constexpr char kContainer1UpdateTxt[] = R"(
  cid: "container1"
  name: "container1"
  namespace: "ns0"
  start_timestamp_ns: 100
  pod_id: "pod0"
  pod_name: "pod0"
)";

class ResolveTargetObjPathTest : public ::testing::Test {
 protected:
  void SetUp() override {
    auto server_path = pl::testing::BazelBinTestFilePath(kServerPath).string();
    ASSERT_OK(s_.Start({server_path}));

    md::K8sMetadataState::PodUpdate pod0_update;
    md::K8sMetadataState::PodUpdate pod1_update;
    md::K8sMetadataState::ContainerUpdate container0_update;
    md::K8sMetadataState::ContainerUpdate container1_update;

    ASSERT_TRUE(TextFormat::ParseFromString(kPod0UpdateTxt, &pod0_update));
    ASSERT_TRUE(TextFormat::ParseFromString(kPod1UpdateTxt, &pod1_update));
    ASSERT_TRUE(TextFormat::ParseFromString(kContainer0UpdateTxt, &container0_update));
    ASSERT_TRUE(TextFormat::ParseFromString(kContainer1UpdateTxt, &container1_update));

    ASSERT_OK(k8s_mds_.HandleContainerUpdate(container0_update));
    ASSERT_OK(k8s_mds_.HandleContainerUpdate(container1_update));
    ASSERT_OK(k8s_mds_.HandlePodUpdate(pod0_update));
    ASSERT_OK(k8s_mds_.HandlePodUpdate(pod1_update));

    k8s_mds_.containers_by_id()["container0"]->AddUPID(PIDToUPID(s_.child_pid()));
  }

  void TearDown() override {
    s_.Kill();
    EXPECT_EQ(9, s_.Wait()) << "Server should have been killed.";
  }

  SubProcess s_;
  md::K8sMetadataState k8s_mds_;
};

TEST_F(ResolveTargetObjPathTest, ResolveUPID) {
  ir::shared::DeploymentSpec deployment_spec;
  deployment_spec.mutable_upid()->set_pid(s_.child_pid());

  ASSERT_OK(ResolveTargetObjPath(k8s_mds_, &deployment_spec));
  EXPECT_THAT(deployment_spec.path(), EndsWith(kServerPath));
  EXPECT_OK(fs::Exists(deployment_spec.path()));
}

TEST_F(ResolveTargetObjPathTest, ResolvePodProcessSuccess) {
  ir::shared::DeploymentSpec deployment_spec;
  constexpr char kDeploymentSpecTxt[] = R"(
    pod_process {
      pod: "ns0/pod0"
      container: "container0"
      process: "go_grpc_server"
    }
  )";
  TextFormat::ParseFromString(kDeploymentSpecTxt, &deployment_spec);
  ASSERT_OK(ResolveTargetObjPath(k8s_mds_, &deployment_spec));
  EXPECT_THAT(deployment_spec.path(), EndsWith(kServerPath));
  EXPECT_OK(fs::Exists(deployment_spec.path()));
}

// Tests that non-matching process regexp returns no UPID.
TEST_F(ResolveTargetObjPathTest, ResolvePodProcessNonMatchingProcessRegexp) {
  ir::shared::DeploymentSpec deployment_spec;
  constexpr char kDeploymentSpecTxt[] = R"(
    pod_process {
      pod: "ns0/pod0"
      container: "container0"
      process: "non-existent-regexp"
    }
  )";
  TextFormat::ParseFromString(kDeploymentSpecTxt, &deployment_spec);
  EXPECT_THAT(
      ResolveTargetObjPath(k8s_mds_, &deployment_spec),
      StatusIs(pl::statuspb::NOT_FOUND, HasSubstr("Found no UPIDs for Container: 'container0'")));
}

// Tests that a given pod name prefix matches multiple Pods.
TEST_F(ResolveTargetObjPathTest, ResolvePodProcessMultiplePods) {
  ir::shared::DeploymentSpec deployment_spec;
  constexpr char kDeploymentSpecTxt[] = R"(
    pod_process {
      pod: "ns0/pod"
    }
  )";
  TextFormat::ParseFromString(kDeploymentSpecTxt, &deployment_spec);
  EXPECT_THAT(ResolveTargetObjPath(k8s_mds_, &deployment_spec),
              StatusIs(pl::statuspb::INVALID_ARGUMENT,
                       HasSubstr("Pod name prefix 'ns0/pod' matches multiple Pods:")));
}

// Tests that empty container name results into failure when there are multiple containers in Pod.
TEST_F(ResolveTargetObjPathTest, ResolvePodProcessMissingContainerName) {
  ir::shared::DeploymentSpec deployment_spec;
  constexpr char kDeploymentSpecTxt[] = R"(
    pod_process {
      pod: "ns0/pod0"
    }
  )";
  TextFormat::ParseFromString(kDeploymentSpecTxt, &deployment_spec);
  EXPECT_THAT(ResolveTargetObjPath(k8s_mds_, &deployment_spec),
              StatusIs(pl::statuspb::INVALID_ARGUMENT,
                       HasSubstr("Container not specified, but Pod 'pod0' has multiple "
                                 "containers 'container0,container1'")));
}

constexpr std::string_view kLogicalProgramSpec = R"(
deployment_spec {
  path: "$0"
}
tracepoints {
  program {
    language: GOLANG
    outputs {
      name: "probe_output"
      fields: "f1"
      fields: "f2"
      fields: "f3"
      fields: "f4"
      fields: "latency"
    }
    probes: {
      name: "probe0"
      tracepoint: {
        symbol: "main.MixedArgTypes"
        type: LOGICAL
      }
      args {
        id: "arg0"
        expr: "i1"
      }
        args {
        id: "arg1"
        expr: "i2"
      }
      args {
        id: "arg2"
        expr: "i3"
      }
      ret_vals {
        id: "retval0"
        expr: "$$6"
      }
      function_latency { id: "latency" }
      output_actions {
        output_name: "probe_output"
        variable_name: "arg0"
        variable_name: "arg1"
        variable_name: "arg2"
        variable_name: "retval0"
        variable_name: "latency"
      }
    }
  }
}
)";

TEST(DynamicTracerTest, Compile) {
  std::string input_program_str = absl::Substitute(
      kLogicalProgramSpec, pl::testing::BazelBinTestFilePath(kBinaryPath).string());
  ir::logical::TracepointDeployment input_program;
  ASSERT_TRUE(TextFormat::ParseFromString(input_program_str, &input_program));

  ASSERT_OK_AND_ASSIGN(BCCProgram bcc_program, CompileProgram(&input_program));

  ASSERT_THAT(bcc_program.uprobe_specs, SizeIs(4));

  const auto& spec = bcc_program.uprobe_specs[0];

  EXPECT_THAT(spec, Field(&UProbeSpec::binary_path, ::testing::EndsWith("dummy_go_binary")));
  EXPECT_THAT(spec, Field(&UProbeSpec::symbol, "runtime.casgstatus"));
  EXPECT_THAT(spec, Field(&UProbeSpec::attach_type, bpf_tools::BPFProbeAttachType::kEntry));
  EXPECT_THAT(spec, Field(&UProbeSpec::probe_fn, "probe_entry_runtime_casgstatus"));

  ASSERT_THAT(bcc_program.perf_buffer_specs, SizeIs(1));

  const auto& perf_buffer_name = bcc_program.perf_buffer_specs[0].name;
  const auto& perf_buffer_output = bcc_program.perf_buffer_specs[0].output;

  EXPECT_THAT(perf_buffer_name, "probe_output");
  EXPECT_THAT(perf_buffer_output, EqualsProto(
                                      R"proto(
                                      name: "probe_output_value_t"
                                      fields {
                                        name: "tgid_"
                                        type: INT32
                                      }
                                      fields {
                                        name: "tgid_start_time_"
                                        type: UINT64
                                      }
                                      fields {
                                        name: "time_"
                                        type: UINT64
                                      }
                                      fields {
                                        name: "goid_"
                                        type: INT64
                                      }
                                      fields {
                                        name: "f1"
                                        type: INT
                                      }
                                      fields {
                                        name: "f2"
                                        type: INT
                                      }
                                      fields {
                                        name: "f3"
                                        type: INT
                                      }
                                      fields {
                                        name: "f4"
                                        type: INT
                                      }
                                      fields {
                                        name: "latency"
                                        type: INT64
                                      }
                                      )proto"));
}

}  // namespace dynamic_tracing
}  // namespace stirling
}  // namespace pl
