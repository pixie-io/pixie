/*
 * Copyright 2018- The Pixie Authors.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "src/stirling/source_connectors/dynamic_tracer/dynamic_tracing/dynamic_tracer.h"

#include <string>
#include <vector>

#include "src/common/exec/subprocess.h"
#include "src/common/fs/fs_wrapper.h"
#include "src/common/testing/testing.h"
#include "src/stirling/testing/common.h"

constexpr std::string_view kBinaryPath =
    "src/stirling/obj_tools/testdata/dummy_go_binary_/dummy_go_binary";

namespace px {
namespace stirling {
namespace dynamic_tracing {

using ::google::protobuf::TextFormat;
using ::px::stirling::bpf_tools::UProbeSpec;
using ::px::stirling::testing::PIDToUPID;
using ::px::testing::proto::EqualsProto;
using ::px::testing::status::StatusIs;
using ::testing::ElementsAreArray;
using ::testing::EndsWith;
using ::testing::Field;
using ::testing::HasSubstr;
using ::testing::SizeIs;

constexpr char kServerPath[] =
    "src/stirling/source_connectors/socket_tracer/protocols/http2/testing/go_grpc_server/"
    "go_grpc_server_/"
    "go_grpc_server";

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
    auto server_path = px::testing::BazelBinTestFilePath(kServerPath).string();
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

    k8s_mds_.containers_by_id()["container0"]->mutable_active_upids()->emplace(
        PIDToUPID(s_.child_pid()));
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
      StatusIs(px::statuspb::NOT_FOUND, HasSubstr("Found no UPIDs in Container: 'container0'")));
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
              StatusIs(px::statuspb::FAILED_PRECONDITION,
                       HasSubstr("Pod name 'ns0/pod' matches multiple Pods")));
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
              StatusIs(px::statuspb::FAILED_PRECONDITION,
                       HasSubstr("Container name not specified, but Pod 'pod0' has multiple "
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
        expr: "$$0"
      }
      function_latency { id: "latency" }
      output_actions {
        output_name: "probe_output"
        variable_names: "arg0"
        variable_names: "arg1"
        variable_names: "arg2"
        variable_names: "retval0"
        variable_names: "latency"
      }
    }
  }
}
)";

const std::vector<std::string> kExpectedBCC = {
    "#include <linux/sched.h>",
    "#define __inline inline __attribute__((__always_inline__))",
    "static __inline uint64_t pl_nsec_to_clock_t(uint64_t x) {",
    "return div_u64(x, NSEC_PER_SEC / USER_HZ);",
    "}",
    "static __inline uint64_t pl_tgid_start_time() {",
    "struct task_struct* task_group_leader = ((struct "
    "task_struct*)bpf_get_current_task())->group_leader;",
    "#if LINUX_VERSION_CODE >= 328960",
    "return pl_nsec_to_clock_t(task_group_leader->start_boottime);",
    "#else",
    "return pl_nsec_to_clock_t(task_group_leader->real_start_time);",
    "#endif",
    "}",
    "struct blob32 {",
    "  uint64_t len;",
    "  uint8_t buf[32-sizeof(uint64_t)-1];",
    "  uint8_t truncated;",
    "};",
    "struct blob64 {",
    "  uint64_t len;",
    "  uint8_t buf[64-sizeof(uint64_t)-1];",
    "  uint8_t truncated;",
    "};",
    "struct struct_blob64 {",
    "  uint64_t len;",
    "  int8_t decoder_idx;",
    "  uint8_t buf[64-sizeof(uint64_t)-2];",
    "  uint8_t truncated;",
    "};",
    "struct pid_goid_map_value_t {",
    "  int64_t goid;",
    "} __attribute__((packed, aligned(1)));",
    "struct probe0_argstash_value_t {",
    "  int arg0;",
    "  int arg1;",
    "  int arg2;",
    "  uint64_t time_;",
    "} __attribute__((packed, aligned(1)));",
    "struct probe_output_value_t {",
    "  int32_t tgid_;",
    "  uint64_t tgid_start_time_;",
    "  uint64_t time_;",
    "  int64_t goid_;",
    "  int f1;",
    "  int f2;",
    "  int f3;",
    "  int f4;",
    "  int64_t latency;",
    "} __attribute__((packed, aligned(1)));",
    "BPF_HASH(pid_goid_map, uint64_t, struct pid_goid_map_value_t);",
    "BPF_HASH(probe0_argstash, uint64_t, struct probe0_argstash_value_t);",
    "BPF_PERCPU_ARRAY(probe_output_data_buffer_array, struct probe_output_value_t, 1);",
    "static __inline int64_t pl_goid() {",
    "uint64_t current_pid_tgid = bpf_get_current_pid_tgid();",
    "const struct pid_goid_map_value_t* goid_ptr = pid_goid_map.lookup(&current_pid_tgid);",
    "return (goid_ptr == NULL) ? -1 : goid_ptr->goid;",
    "}",
    "BPF_PERF_OUTPUT(probe_output);",
    "int probe_entry_runtime_casgstatus(struct pt_regs* ctx) {",
    "void* sp_ = (void*)PT_REGS_SP(ctx);",
    "int32_t tgid_ = bpf_get_current_pid_tgid() >> 32;",
    "uint64_t tgid_pid_ = bpf_get_current_pid_tgid();",
    "uint64_t tgid_start_time_ = pl_tgid_start_time();",
    "uint64_t time_ = bpf_ktime_get_ns();",
    "int64_t goid_ = pl_goid();",
    "int64_t kGRunningState = 2;",
    "void* goid_X_;",
    "bpf_probe_read(&goid_X_, sizeof(void*), sp_ + 8);",
    "int64_t goid;",
    "bpf_probe_read(&goid, sizeof(int64_t), goid_X_ + 152);",
    "uint32_t newval;",
    "bpf_probe_read(&newval, sizeof(uint32_t), sp_ + 20);",
    "struct pid_goid_map_value_t pid_goid_map_value = {};",
    "pid_goid_map_value.goid = goid;",
    "if (newval == kGRunningState) {",
    "pid_goid_map.update(&tgid_pid_, &pid_goid_map_value);",
    "}",
    "return 0;",
    "}",
    "int probe0_entry(struct pt_regs* ctx) {",
    "void* sp_ = (void*)PT_REGS_SP(ctx);",
    "int32_t tgid_ = bpf_get_current_pid_tgid() >> 32;",
    "uint64_t tgid_pid_ = bpf_get_current_pid_tgid();",
    "uint64_t tgid_start_time_ = pl_tgid_start_time();",
    "uint64_t time_ = bpf_ktime_get_ns();",
    "int64_t goid_ = pl_goid();",
    "int arg0;",
    "bpf_probe_read(&arg0, sizeof(int), sp_ + 8);",
    "int arg1;",
    "bpf_probe_read(&arg1, sizeof(int), sp_ + 24);",
    "int arg2;",
    "bpf_probe_read(&arg2, sizeof(int), sp_ + 32);",
    "struct probe0_argstash_value_t probe0_argstash_value = {};",
    "probe0_argstash_value.arg0 = arg0;",
    "probe0_argstash_value.arg1 = arg1;",
    "probe0_argstash_value.arg2 = arg2;",
    "probe0_argstash_value.time_ = time_;",
    "probe0_argstash.update(&goid_, &probe0_argstash_value);",
    "return 0;",
    "}",
    "int probe0_return(struct pt_regs* ctx) {",
    "void* sp_ = (void*)PT_REGS_SP(ctx);",
    "int32_t tgid_ = bpf_get_current_pid_tgid() >> 32;",
    "uint64_t tgid_pid_ = bpf_get_current_pid_tgid();",
    "uint64_t tgid_start_time_ = pl_tgid_start_time();",
    "uint64_t time_ = bpf_ktime_get_ns();",
    "int64_t goid_ = pl_goid();",
    "int retval0;",
    "bpf_probe_read(&retval0, sizeof(int), sp_ + 48);",
    "struct probe0_argstash_value_t* probe0_argstash_ptr = probe0_argstash.lookup(&goid_);",
    "if (probe0_argstash_ptr == NULL) { return 0; }",
    "int arg0 = probe0_argstash_ptr->arg0;",
    "if (probe0_argstash_ptr == NULL) { return 0; }",
    "int arg1 = probe0_argstash_ptr->arg1;",
    "if (probe0_argstash_ptr == NULL) { return 0; }",
    "int arg2 = probe0_argstash_ptr->arg2;",
    "if (probe0_argstash_ptr == NULL) { return 0; }",
    "uint64_t start_ktime_ns = probe0_argstash_ptr->time_;",
    "int64_t latency = time_ - start_ktime_ns;",
    "probe0_argstash.delete(&goid_);",
    "uint32_t probe_output_value_idx = 0;",
    "struct probe_output_value_t* probe_output_value = "
    "probe_output_data_buffer_array.lookup(&probe_output_value_idx);",
    "if (probe_output_value == NULL) { return 0; }",
    "probe_output_value->tgid_ = tgid_;",
    "probe_output_value->tgid_start_time_ = tgid_start_time_;",
    "probe_output_value->time_ = time_;",
    "probe_output_value->goid_ = goid_;",
    "probe_output_value->f1 = arg0;",
    "probe_output_value->f2 = arg1;",
    "probe_output_value->f3 = arg2;",
    "probe_output_value->f4 = retval0;",
    "probe_output_value->latency = latency;",
    "probe_output.perf_submit(ctx, probe_output_value, sizeof(*probe_output_value));",
    "return 0;",
    "}",
};

TEST(DynamicTracerTest, Compile) {
  std::string input_program_str = absl::Substitute(
      kLogicalProgramSpec, px::testing::BazelBinTestFilePath(kBinaryPath).string());
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
  std::vector<std::string> code_lines = absl::StrSplit(bcc_program.code, "\n");

  EXPECT_THAT(code_lines, ElementsAreArray(kExpectedBCC));
}

}  // namespace dynamic_tracing
}  // namespace stirling
}  // namespace px
