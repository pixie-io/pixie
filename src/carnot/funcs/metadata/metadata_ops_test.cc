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

#include <rapidjson/document.h>

#include <memory>
#include <numeric>
#include <type_traits>
#include <utility>
#include <vector>

#include "src/carnot/funcs/metadata/metadata_ops.h"
#include "src/carnot/udf/test_utils.h"
#include "src/common/base/base.h"
#include "src/common/testing/testing.h"
#include "src/shared/k8s/metadatapb/test_proto.h"
#include "src/shared/metadata/pids.h"
#include "src/shared/metadata/state_manager.h"
#include "src/shared/metadata/test_utils.h"
#include "src/shared/upid/upid.h"

namespace px {
namespace carnot {
namespace funcs {
namespace metadata {

using ::px::carnot::udf::FunctionContext;

using ResourceUpdate = px::shared::k8s::metadatapb::ResourceUpdate;
using ::testing::AnyOf;

class MetadataOpsTest : public ::testing::Test {
 protected:
  void SetUp() override {
    agent_id_ = sole::uuid4();
    metadata_state_ =
        std::make_shared<px::md::AgentMetadataState>(/* hostname */ "myhost",
                                                     /* asid */ 1, agent_id_, "mypod");
    // Apply updates to metadata state.
    updates_ =
        std::make_unique<moodycamel::BlockingConcurrentQueue<std::unique_ptr<ResourceUpdate>>>();

    updates_->enqueue(px::metadatapb::testutils::CreateRunningContainerUpdatePB());
    updates_->enqueue(px::metadatapb::testutils::CreateRunningPodUpdatePB());
    updates_->enqueue(px::metadatapb::testutils::CreateRunningServiceUpdatePB());
    updates_->enqueue(px::metadatapb::testutils::CreateTerminatingContainerUpdatePB());
    updates_->enqueue(px::metadatapb::testutils::CreateTerminatingPodUpdatePB());
    updates_->enqueue(px::metadatapb::testutils::CreateTerminatingServiceUpdatePB());

    auto s = px::md::ApplyK8sUpdates(10, metadata_state_.get(), &md_filter_, updates_.get());

    // Apply PID updates to metadata state.
    auto upid1 = md::UPID(123, 567, 89101);
    auto pid1 = std::make_unique<md::PIDInfo>(upid1, "test", "pod1_container_1");
    metadata_state_->AddUPID(upid1, std::move(pid1));
    auto upid2 = md::UPID(123, 567, 468);
    auto pid2 = std::make_unique<md::PIDInfo>(upid2, "cmdline", "pod2_container_1");
    metadata_state_->AddUPID(upid2, std::move(pid2));

    ASSERT_OK(s);
  }
  sole::uuid agent_id_;
  std::shared_ptr<px::md::AgentMetadataState> metadata_state_;
  std::unique_ptr<moodycamel::BlockingConcurrentQueue<std::unique_ptr<ResourceUpdate>>> updates_;
  md::TestAgentMetadataFilter md_filter_;
};

TEST_F(MetadataOpsTest, asid_test) {
  auto function_ctx = std::make_unique<FunctionContext>(metadata_state_, nullptr);
  auto udf_tester = px::carnot::udf::UDFTester<ASIDUDF>(std::move(function_ctx));
  udf_tester.ForInput().Expect(1);
}

TEST_F(MetadataOpsTest, upid_to_asid_test) {
  auto function_ctx = std::make_unique<FunctionContext>(metadata_state_, nullptr);
  auto upid = types::UInt128Value(528280977975, 89101);
  auto udf_tester = px::carnot::udf::UDFTester<UPIDToASIDUDF>(std::move(function_ctx));
  udf_tester.ForInput(upid).Expect(123);
}

TEST_F(MetadataOpsTest, pod_id_to_pod_name_test) {
  auto function_ctx = std::make_unique<FunctionContext>(metadata_state_, nullptr);
  auto udf_tester = px::carnot::udf::UDFTester<PodIDToPodNameUDF>(std::move(function_ctx));
  udf_tester.ForInput("1_uid").Expect("pl/running_pod");
  udf_tester.ForInput("2_uid").Expect("pl/terminating_pod");
  udf_tester.ForInput("missing").Expect("");
}

TEST_F(MetadataOpsTest, pod_name_to_pod_id_test) {
  auto function_ctx = std::make_unique<FunctionContext>(metadata_state_, nullptr);
  auto udf_tester = px::carnot::udf::UDFTester<PodNameToPodIDUDF>(std::move(function_ctx));
  udf_tester.ForInput("pl/running_pod").Expect("1_uid");
  udf_tester.ForInput("pl/terminating_pod").Expect("2_uid");
  udf_tester.ForInput("pl/missing").Expect("");
}

TEST_F(MetadataOpsTest, upid_to_pod_id_test) {
  auto function_ctx = std::make_unique<FunctionContext>(metadata_state_, nullptr);
  auto udf_tester = px::carnot::udf::UDFTester<UPIDToPodIDUDF>(std::move(function_ctx));
  auto upid1 = types::UInt128Value(528280977975, 89101);
  udf_tester.ForInput(upid1).Expect("1_uid");
  auto upid2 = types::UInt128Value(528280977975, 468);
  udf_tester.ForInput(upid2).Expect("2_uid");
  auto upid3 = types::UInt128Value(528280977975, 123);
  udf_tester.ForInput(upid3).Expect("");
}

TEST_F(MetadataOpsTest, upid_to_pod_name_test) {
  auto function_ctx = std::make_unique<FunctionContext>(metadata_state_, nullptr);
  auto udf_tester = px::carnot::udf::UDFTester<UPIDToPodNameUDF>(std::move(function_ctx));
  auto upid1 = types::UInt128Value(528280977975, 89101);
  udf_tester.ForInput(upid1).Expect("pl/running_pod");
  auto upid2 = types::UInt128Value(528280977975, 468);
  udf_tester.ForInput(upid2).Expect("pl/terminating_pod");
  auto upid3 = types::UInt128Value(528280977975, 123);
  udf_tester.ForInput(upid3).Expect("");
}

TEST_F(MetadataOpsTest, upid_to_namespace_test) {
  auto function_ctx = std::make_unique<FunctionContext>(metadata_state_, nullptr);
  auto udf_tester = px::carnot::udf::UDFTester<UPIDToNamespaceUDF>(std::move(function_ctx));
  auto upid1 = types::UInt128Value(528280977975, 89101);
  udf_tester.ForInput(upid1).Expect("pl");
  auto upid2 = types::UInt128Value(528280977975, 468);
  udf_tester.ForInput(upid2).Expect("pl");
  auto upid3 = types::UInt128Value(528280977975, 123);
  udf_tester.ForInput(upid3).Expect("");
}

TEST_F(MetadataOpsTest, upid_to_container_id_test) {
  auto function_ctx = std::make_unique<FunctionContext>(metadata_state_, nullptr);
  auto udf_tester = px::carnot::udf::UDFTester<UPIDToContainerIDUDF>(std::move(function_ctx));
  auto upid1 = types::UInt128Value(528280977975, 89101);
  udf_tester.ForInput(upid1).Expect("pod1_container_1");
  auto upid2 = types::UInt128Value(528280977975, 468);
  udf_tester.ForInput(upid2).Expect("pod2_container_1");
  auto upid3 = types::UInt128Value(528280977975, 123);
  udf_tester.ForInput(upid3).Expect("");
}

TEST_F(MetadataOpsTest, upid_to_container_name_test) {
  auto function_ctx = std::make_unique<FunctionContext>(metadata_state_, nullptr);
  auto udf_tester = px::carnot::udf::UDFTester<UPIDToContainerNameUDF>(std::move(function_ctx));
  auto upid1 = types::UInt128Value(528280977975, 89101);
  udf_tester.ForInput(upid1).Expect("running_container");
  auto upid2 = types::UInt128Value(528280977975, 468);
  udf_tester.ForInput(upid2).Expect("terminating_container");
  auto upid3 = types::UInt128Value(528280977975, 123);
  udf_tester.ForInput(upid3).Expect("");
}

TEST_F(MetadataOpsTest, upid_to_service_id_test) {
  auto function_ctx = std::make_unique<FunctionContext>(metadata_state_, nullptr);
  auto udf_tester = px::carnot::udf::UDFTester<UPIDToServiceIDUDF>(std::move(function_ctx));
  auto upid1 = types::UInt128Value(528280977975, 89101);
  udf_tester.ForInput(upid1).Expect("3_uid");
  auto upid2 = types::UInt128Value(528280977975, 468);
  udf_tester.ForInput(upid2).Expect("4_uid");
  auto upid3 = types::UInt128Value(528280977975, 123);
  udf_tester.ForInput(upid3).Expect("");

  // Terminate a service, and make sure that the upid no longer associates with that service.
  updates_->enqueue(px::metadatapb::testutils::CreateTerminatedServiceUpdatePB());
  EXPECT_OK(px::md::ApplyK8sUpdates(11, metadata_state_.get(), &md_filter_, updates_.get()));
  // upid2 previously was connected to 4_uid.
  udf_tester.ForInput(upid2).Expect("");
}

TEST_F(MetadataOpsTest, upid_to_service_name_test) {
  auto function_ctx = std::make_unique<FunctionContext>(metadata_state_, nullptr);
  auto udf_tester = px::carnot::udf::UDFTester<UPIDToServiceNameUDF>(std::move(function_ctx));
  auto upid1 = types::UInt128Value(528280977975, 89101);
  udf_tester.ForInput(upid1).Expect("pl/running_service");
  auto upid2 = types::UInt128Value(528280977975, 468);
  udf_tester.ForInput(upid2).Expect("pl/terminating_service");
  auto upid3 = types::UInt128Value(528280977975, 123);
  udf_tester.ForInput(upid3).Expect("");

  updates_->enqueue(px::metadatapb::testutils::CreateTerminatedServiceUpdatePB());
  EXPECT_OK(px::md::ApplyK8sUpdates(11, metadata_state_.get(), &md_filter_, updates_.get()));
  // upid2 previously was connected to pl/terminating_service.
  udf_tester.ForInput(upid2).Expect("");
}

TEST_F(MetadataOpsTest, upid_to_node_name_test) {
  auto function_ctx = std::make_unique<FunctionContext>(metadata_state_, nullptr);
  auto udf_tester = px::carnot::udf::UDFTester<UPIDToNodeNameUDF>(std::move(function_ctx));
  auto upid1 = types::UInt128Value(528280977975, 89101);
  udf_tester.ForInput(upid1).Expect("test_node");
  auto upid2 = types::UInt128Value(528280977975, 468);
  udf_tester.ForInput(upid2).Expect("test_node_tbt");
  auto upid3 = types::UInt128Value(528280977975, 123);
  udf_tester.ForInput(upid3).Expect("");

  updates_->enqueue(px::metadatapb::testutils::CreateTerminatedPodUpdatePB());
  EXPECT_OK(px::md::ApplyK8sUpdates(11, metadata_state_.get(), &md_filter_, updates_.get()));
  // upid2 previously was connected to pl/terminating_pod.
  udf_tester.ForInput(upid2).Expect("");
}

TEST_F(MetadataOpsTest, pod_id_to_node_name_test) {
  auto function_ctx = std::make_unique<FunctionContext>(metadata_state_, nullptr);
  auto udf_tester = px::carnot::udf::UDFTester<PodIDToNodeNameUDF>(std::move(function_ctx));
  udf_tester.ForInput("1_uid").Expect("test_node");
  // This pod is not available, should return empty.
  udf_tester.ForInput("123_uid").Expect("");
}

TEST_F(MetadataOpsTest, upid_to_hostname_test) {
  auto function_ctx = std::make_unique<FunctionContext>(metadata_state_, nullptr);
  auto udf_tester = px::carnot::udf::UDFTester<UPIDToHostnameUDF>(std::move(function_ctx));
  auto upid1 = types::UInt128Value(528280977975, 89101);
  udf_tester.ForInput(upid1).Expect("test_host");
  auto upid2 = types::UInt128Value(528280977975, 468);
  udf_tester.ForInput(upid2).Expect("test_host_tbt");
  auto upid3 = types::UInt128Value(528280977975, 123);
  udf_tester.ForInput(upid3).Expect("");

  updates_->enqueue(px::metadatapb::testutils::CreateTerminatedPodUpdatePB());
  EXPECT_OK(px::md::ApplyK8sUpdates(11, metadata_state_.get(), &md_filter_, updates_.get()));
  // upid2 previously was connected to pl/terminating_pod.
  udf_tester.ForInput(upid2).Expect("");
}

TEST_F(MetadataOpsTest, service_id_to_service_name_test) {
  auto function_ctx = std::make_unique<FunctionContext>(metadata_state_, nullptr);
  auto udf_tester = px::carnot::udf::UDFTester<ServiceIDToServiceNameUDF>(std::move(function_ctx));
  udf_tester.ForInput("3_uid").Expect("pl/running_service");
  udf_tester.ForInput("4_uid").Expect("pl/terminating_service");
  udf_tester.ForInput("nonexistent").Expect("");
}

TEST_F(MetadataOpsTest, service_name_to_service_id_test) {
  auto function_ctx = std::make_unique<FunctionContext>(metadata_state_, nullptr);
  auto udf_tester = px::carnot::udf::UDFTester<ServiceNameToServiceIDUDF>(std::move(function_ctx));
  udf_tester.ForInput("pl/running_service").Expect("3_uid");
  // Terminating service has not yet terminated.
  udf_tester.ForInput("pl/terminating_service").Expect("4_uid");
  udf_tester.ForInput("pl/nonexistent").Expect("");
  udf_tester.ForInput("bad_format").Expect("");
}

TEST_F(MetadataOpsTest, upid_to_service_id_test_multiple_services) {
  updates_->enqueue(px::metadatapb::testutils::CreateServiceWithSamePodUpdatePB());
  EXPECT_OK(px::md::ApplyK8sUpdates(11, metadata_state_.get(), &md_filter_, updates_.get()));
  auto function_ctx = std::make_unique<FunctionContext>(metadata_state_, nullptr);
  // auto udf_tester = px::carnot::udf::UDFTester<UPIDToServiceIDUDF>(std::move(function_ctx));
  UPIDToServiceIDUDF udf;
  auto upid1 = types::UInt128Value(528280977975, 89101);
  EXPECT_THAT(udf.Exec(function_ctx.get(), upid1),
              AnyOf("[\"3_uid\",\"5_uid\"]", "[\"5_uid\",\"3_uid\"]"));
}

TEST_F(MetadataOpsTest, upid_to_service_name_test_multiple_services) {
  updates_->enqueue(px::metadatapb::testutils::CreateServiceWithSamePodUpdatePB());
  EXPECT_OK(px::md::ApplyK8sUpdates(11, metadata_state_.get(), &md_filter_, updates_.get()));
  auto function_ctx = std::make_unique<FunctionContext>(metadata_state_, nullptr);
  UPIDToServiceNameUDF udf;
  auto upid1 = types::UInt128Value(528280977975, 89101);
  EXPECT_THAT(udf.Exec(function_ctx.get(), upid1),
              AnyOf("[\"pl/running_service\",\"pl/other_service_with_pod\"]",
                    "[\"pl/other_service_with_pod\",\"pl/running_service\"]"));
}

TEST_F(MetadataOpsTest, pod_id_to_service_name_test_multiple_services) {
  updates_->enqueue(px::metadatapb::testutils::CreateServiceWithSamePodUpdatePB());
  EXPECT_OK(px::md::ApplyK8sUpdates(11, metadata_state_.get(), &md_filter_, updates_.get()));
  auto function_ctx = std::make_unique<FunctionContext>(metadata_state_, nullptr);
  PodIDToServiceNameUDF udf;
  EXPECT_THAT(udf.Exec(function_ctx.get(), "1_uid"),
              AnyOf("[\"pl/running_service\",\"pl/other_service_with_pod\"]",
                    "[\"pl/other_service_with_pod\",\"pl/running_service\"]"));
}

TEST_F(MetadataOpsTest, pod_id_to_service_id_test_multiple_services) {
  updates_->enqueue(px::metadatapb::testutils::CreateServiceWithSamePodUpdatePB());
  EXPECT_OK(px::md::ApplyK8sUpdates(11, metadata_state_.get(), &md_filter_, updates_.get()));
  auto function_ctx = std::make_unique<FunctionContext>(metadata_state_, nullptr);
  PodIDToServiceIDUDF udf;
  EXPECT_THAT(udf.Exec(function_ctx.get(), "1_uid"),
              AnyOf("[\"3_uid\",\"5_uid\"]", "[\"5_uid\",\"3_uid\"]"));
}

TEST_F(MetadataOpsTest, pod_name_to_service_name_test_multiple_services) {
  updates_->enqueue(px::metadatapb::testutils::CreateServiceWithSamePodUpdatePB());
  EXPECT_OK(px::md::ApplyK8sUpdates(11, metadata_state_.get(), &md_filter_, updates_.get()));
  auto function_ctx = std::make_unique<FunctionContext>(metadata_state_, nullptr);
  PodNameToServiceNameUDF udf;
  EXPECT_THAT(udf.Exec(function_ctx.get(), "pl/running_pod"),
              AnyOf("[\"pl/running_service\",\"pl/other_service_with_pod\"]",
                    "[\"pl/other_service_with_pod\",\"pl/running_service\"]"));
}

TEST_F(MetadataOpsTest, pod_name_to_service_id_test_multiple_services) {
  updates_->enqueue(px::metadatapb::testutils::CreateServiceWithSamePodUpdatePB());
  EXPECT_OK(px::md::ApplyK8sUpdates(11, metadata_state_.get(), &md_filter_, updates_.get()));
  auto function_ctx = std::make_unique<FunctionContext>(metadata_state_, nullptr);
  PodNameToServiceIDUDF udf;
  EXPECT_THAT(udf.Exec(function_ctx.get(), "pl/running_pod"),
              AnyOf("[\"3_uid\",\"5_uid\"]", "[\"5_uid\",\"3_uid\"]"));
}

TEST_F(MetadataOpsTest, upid_to_string) {
  UPIDToStringUDF udf;
  auto upid1 = md::UPID(123, 567, 89101);
  EXPECT_EQ(udf.Exec(nullptr, upid1.value()), absl::Substitute("$0:$1:$2", 123, 567, 89101));

  auto upid2 = md::UPID(255, 123, 11111);
  EXPECT_EQ(udf.Exec(nullptr, upid2.value()), absl::Substitute("$0:$1:$2", 255, 123, 11111));
}

TEST_F(MetadataOpsTest, upid_to_pid) {
  UPIDToPIDUDF udf;
  auto upid1 = md::UPID(123, 567, 89101);
  EXPECT_EQ(udf.Exec(nullptr, upid1.value()), 567);

  auto upid2 = md::UPID(255, 123, 11111);
  EXPECT_EQ(udf.Exec(nullptr, upid2.value()), 123);
}

TEST_F(MetadataOpsTest, pod_id_to_start_time) {
  PodIDToPodStartTimeUDF udf;
  auto function_ctx = std::make_unique<FunctionContext>(metadata_state_, nullptr);
  // 1_uid is the Pod id for the currently running pod.
  EXPECT_EQ(udf.Exec(function_ctx.get(), "1_uid").val, 5);
  // 1234567_uid is a nonexistent Pod id, should return 0.
  EXPECT_EQ(udf.Exec(function_ctx.get(), "1234567_uid").val, 0);
}

TEST_F(MetadataOpsTest, pod_id_to_stop_time) {
  PodIDToPodStopTimeUDF udf;
  updates_->enqueue(px::metadatapb::testutils::CreateTerminatedPodUpdatePB());
  EXPECT_OK(px::md::ApplyK8sUpdates(11, metadata_state_.get(), &md_filter_, updates_.get()));

  auto function_ctx = std::make_unique<FunctionContext>(metadata_state_, nullptr);
  // 2_uid is the Pod id for a terminating pod.
  EXPECT_EQ(udf.Exec(function_ctx.get(), "2_uid").val, 15);
  // 1234567_uid is a nonexistent Pod id, should return 0.
  EXPECT_EQ(udf.Exec(function_ctx.get(), "1234567_uid").val, 0);
}

TEST_F(MetadataOpsTest, pod_name_to_start_time) {
  PodNameToPodStartTimeUDF udf;
  auto function_ctx = std::make_unique<FunctionContext>(metadata_state_, nullptr);
  // pl/running_pod is the Pod name for the currently running pod.
  EXPECT_EQ(udf.Exec(function_ctx.get(), "pl/running_pod").val, 5);
  // pl/blah is a nonexistent Pod, should return 0.
  EXPECT_EQ(udf.Exec(function_ctx.get(), "pl/blah").val, 0);
}

TEST_F(MetadataOpsTest, pod_name_to_stop_time) {
  PodNameToPodStopTimeUDF udf;
  updates_->enqueue(px::metadatapb::testutils::CreateTerminatedPodUpdatePB());
  EXPECT_OK(px::md::ApplyK8sUpdates(11, metadata_state_.get(), &md_filter_, updates_.get()));

  auto function_ctx = std::make_unique<FunctionContext>(metadata_state_, nullptr);
  // pl/terminating_pod is the Pod name for a terminating pod.
  EXPECT_EQ(udf.Exec(function_ctx.get(), "pl/terminating_pod").val, 15);
  // pl/blah is a nonexistant Pod, should return 0.
  EXPECT_EQ(udf.Exec(function_ctx.get(), "pl/blah").val, 0);
}

TEST_F(MetadataOpsTest, container_name_to_container_id_test) {
  auto function_ctx = std::make_unique<FunctionContext>(metadata_state_, nullptr);
  auto udf_tester =
      px::carnot::udf::UDFTester<ContainerNameToContainerIDUDF>(std::move(function_ctx));
  udf_tester.ForInput("running_container").Expect("pod1_container_1");
  udf_tester.ForInput("terminating_container").Expect("pod2_container_1");
  udf_tester.ForInput("nonexistent_container").Expect("");
}

TEST_F(MetadataOpsTest, container_id_to_start_time) {
  ContainerIDToContainerStartTimeUDF udf;
  auto function_ctx = std::make_unique<FunctionContext>(metadata_state_, nullptr);
  // pod1_container_1 is the container id for the currently running container.
  EXPECT_EQ(udf.Exec(function_ctx.get(), "pod1_container_1").val, 6);
  // pod1_container_987654 is a nonexistent container id, should return 0.
  EXPECT_EQ(udf.Exec(function_ctx.get(), "pod1_container_987654").val, 0);
}

TEST_F(MetadataOpsTest, container_id_to_stop_time) {
  ContainerIDToContainerStopTimeUDF udf;
  updates_->enqueue(px::metadatapb::testutils::CreateTerminatedContainerUpdatePB());
  EXPECT_OK(px::md::ApplyK8sUpdates(11, metadata_state_.get(), &md_filter_, updates_.get()));

  auto function_ctx = std::make_unique<FunctionContext>(metadata_state_, nullptr);
  // pod2_container_1 is the container id for a terminated container.
  EXPECT_EQ(udf.Exec(function_ctx.get(), "pod2_container_1").val, 14);
  // pod1_container_987654 is a nonexistent container id, should return 0.
  EXPECT_EQ(udf.Exec(function_ctx.get(), "pod1_container_987654").val, 0);
}

TEST_F(MetadataOpsTest, container_name_to_start_time) {
  ContainerNameToContainerStartTimeUDF udf;
  auto function_ctx = std::make_unique<FunctionContext>(metadata_state_, nullptr);
  // running_container is the container name for the currently running container.
  EXPECT_EQ(udf.Exec(function_ctx.get(), "running_container").val, 6);
  // blah_container is a nonexistent container, should return 0.
  EXPECT_EQ(udf.Exec(function_ctx.get(), "blah_container").val, 0);
}

TEST_F(MetadataOpsTest, container_name_to_stop_time) {
  ContainerNameToContainerStopTimeUDF udf;
  updates_->enqueue(px::metadatapb::testutils::CreateTerminatedContainerUpdatePB());
  EXPECT_OK(px::md::ApplyK8sUpdates(11, metadata_state_.get(), &md_filter_, updates_.get()));

  auto function_ctx = std::make_unique<FunctionContext>(metadata_state_, nullptr);
  // terminating_container is the container name for a terminated container.
  EXPECT_EQ(udf.Exec(function_ctx.get(), "terminating_container").val, 14);
  // blah_container is a nonexistent container, should return 0.
  EXPECT_EQ(udf.Exec(function_ctx.get(), "blah_container").val, 0);
}

TEST_F(MetadataOpsTest, pod_name_to_pod_status) {
  PodNameToPodStatusUDF status_udf;

  updates_->enqueue(px::metadatapb::testutils::CreateTerminatedPodUpdatePB());
  EXPECT_OK(px::md::ApplyK8sUpdates(11, metadata_state_.get(), &md_filter_, updates_.get()));
  auto function_ctx = std::make_unique<FunctionContext>(metadata_state_, nullptr);
  // 1_uid is the Pod id for the currently running pod.
  auto running_res = status_udf.Exec(function_ctx.get(), "pl/running_pod");
  auto failed_res = status_udf.Exec(function_ctx.get(), "pl/terminating_pod");
  auto missing_res = status_udf.Exec(function_ctx.get(), "pl/nonexistent");

  rapidjson::Document running;
  running.Parse(running_res.data());
  EXPECT_EQ("Running", std::string(running["phase"].GetString()));
  EXPECT_EQ("Running message", std::string(running["message"].GetString()));
  EXPECT_EQ("Running reason", std::string(running["reason"].GetString()));
  EXPECT_EQ(true, running["ready"].GetBool());

  rapidjson::Document failed;
  failed.Parse(failed_res.data());
  EXPECT_EQ("Failed", std::string(failed["phase"].GetString()));
  EXPECT_EQ("Failed message terminated", std::string(failed["message"].GetString()));
  EXPECT_EQ("Failed reason terminated", std::string(failed["reason"].GetString()));
  EXPECT_EQ(false, failed["ready"].GetBool());

  rapidjson::Document missing;
  missing.Parse(missing_res.data());
  EXPECT_EQ("Unknown", std::string(missing["phase"].GetString()));
  EXPECT_EQ("", std::string(missing["message"].GetString()));
  EXPECT_EQ("", std::string(missing["reason"].GetString()));
  EXPECT_EQ(false, missing["ready"].GetBool());
}

TEST_F(MetadataOpsTest, pod_name_to_pod_ip) {
  auto function_ctx = std::make_unique<FunctionContext>(metadata_state_, nullptr);
  auto udf_tester = px::carnot::udf::UDFTester<PodNameToPodIPUDF>(std::move(function_ctx));
  udf_tester.ForInput("pl/running_pod").Expect("1.1.1.1");
  udf_tester.ForInput("pl/terminating_pod").Expect("");
  // udf_tester.ForInput("bad_format").Expect("");
  udf_tester.ForInput("pl/nonexistent").Expect("");
}

TEST_F(MetadataOpsTest, container_id_to_container_status) {
  ContainerIDToContainerStatusUDF status_udf;

  EXPECT_OK(px::md::ApplyK8sUpdates(11, metadata_state_.get(), &md_filter_, updates_.get()));
  auto function_ctx = std::make_unique<FunctionContext>(metadata_state_, nullptr);

  auto running_res = status_udf.Exec(function_ctx.get(), "pod1_container_1");
  auto terminating_res = status_udf.Exec(function_ctx.get(), "pod2_container_1");
  auto missing_res = status_udf.Exec(function_ctx.get(), "does_not_exist");

  rapidjson::Document running;
  running.Parse(running_res.data());
  EXPECT_EQ("Running", std::string(running["state"].GetString()));
  EXPECT_EQ("Running message", std::string(running["message"].GetString()));
  EXPECT_EQ("Running reason", std::string(running["reason"].GetString()));

  rapidjson::Document terminating;
  terminating.Parse(terminating_res.data());
  EXPECT_EQ("Terminated", std::string(terminating["state"].GetString()));
  EXPECT_EQ("Terminating message pending", std::string(terminating["message"].GetString()));
  EXPECT_EQ("Terminating reason pending", std::string(terminating["reason"].GetString()));

  rapidjson::Document missing;
  missing.Parse(missing_res.data());
  EXPECT_EQ("Unknown", std::string(missing["state"].GetString()));
  EXPECT_EQ("", std::string(missing["message"].GetString()));
  EXPECT_EQ("", std::string(missing["reason"].GetString()));
}

TEST_F(MetadataOpsTest, upid_to_cmdline) {
  auto function_ctx = std::make_unique<FunctionContext>(metadata_state_, nullptr);

  UPIDToCmdLineUDF udf;
  auto upid1 = md::UPID(123, 567, 89101);
  EXPECT_EQ(udf.Exec(function_ctx.get(), upid1.value()), "test");
  auto upid2 = md::UPID(123, 567, 468);
  EXPECT_EQ(udf.Exec(function_ctx.get(), upid2.value()), "cmdline");
  auto upid3 = md::UPID(111, 222, 333);
  EXPECT_EQ(udf.Exec(function_ctx.get(), upid3.value()), "");
}

TEST_F(MetadataOpsTest, hostname) {
  auto function_ctx = std::make_unique<FunctionContext>(metadata_state_, nullptr);

  HostnameUDF udf;
  EXPECT_EQ(udf.Exec(function_ctx.get()), "myhost");
}

TEST_F(MetadataOpsTest, num_cpus) {
  auto function_ctx = std::make_unique<FunctionContext>(metadata_state_, nullptr);

  HostNumCPUsUDF udf;
  auto num_cpus = udf.Exec(function_ctx.get());
  // If this test is ever run on a host with more than 1024 CPUs,
  // then first off...congratulations! Now please update this expectation :)
  EXPECT_GT(num_cpus, 0);
  EXPECT_LT(num_cpus, 1024);
}

TEST_F(MetadataOpsTest, pod_ip) {
  auto function_ctx = std::make_unique<FunctionContext>(metadata_state_, nullptr);

  PodIPToPodIDUDF udf;
  EXPECT_EQ(udf.Exec(function_ctx.get(), "1.1.1.1"), "1_uid");
  EXPECT_EQ(udf.Exec(function_ctx.get(), "1.2.1.2"), "");
}

TEST_F(MetadataOpsTest, pod_ip_to_service_id_test) {
  auto function_ctx = std::make_unique<FunctionContext>(metadata_state_, nullptr);
  PodIPToServiceIDUDF udf;
  EXPECT_EQ(udf.Exec(function_ctx.get(), "1.1.1.1"), "3_uid");
  EXPECT_EQ(udf.Exec(function_ctx.get(), "1.1.1.10"), "");
  EXPECT_EQ(udf.Exec(function_ctx.get(), "1.2.1.2"), "");
}

TEST_F(MetadataOpsTest, upid_to_qos) {
  auto function_ctx = std::make_unique<FunctionContext>(metadata_state_, nullptr);
  auto udf_tester = px::carnot::udf::UDFTester<UPIDToPodQoSUDF>(std::move(function_ctx));
  auto upid1 = types::UInt128Value(528280977975, 89101);
  udf_tester.ForInput(upid1).Expect("kGuaranteed");
  auto upid2 = types::UInt128Value(528280977975, 468);
  udf_tester.ForInput(upid2).Expect("kBestEffort");
  auto upid3 = types::UInt128Value(528280977975, 123);
  udf_tester.ForInput(upid3).Expect("");
}

TEST_F(MetadataOpsTest, upid_to_pod_status) {
  UPIDToPodStatusUDF udf;
  updates_->enqueue(px::metadatapb::testutils::CreateTerminatedPodUpdatePB());
  EXPECT_OK(px::md::ApplyK8sUpdates(11, metadata_state_.get(), &md_filter_, updates_.get()));
  auto function_ctx = std::make_unique<FunctionContext>(metadata_state_, nullptr);
  // 1_uid is the Pod id for the currently running pod.
  auto upid1 = types::UInt128Value(528280977975, 89101);
  auto running_res = udf.Exec(function_ctx.get(), upid1);
  auto upid2 = types::UInt128Value(528280977975, 468);
  auto failed_res = std::string(udf.Exec(function_ctx.get(), upid2));
  auto upid3 = types::UInt128Value(528280866988, 111);
  auto missing_res = std::string(udf.Exec(function_ctx.get(), upid3));

  rapidjson::Document running;
  running.Parse(running_res.data());
  EXPECT_EQ("Running", std::string(running["phase"].GetString()));
  EXPECT_EQ("Running message", std::string(running["message"].GetString()));
  EXPECT_EQ("Running reason", std::string(running["reason"].GetString()));

  rapidjson::Document failed;
  failed.Parse(failed_res.data());
  EXPECT_EQ("Failed", std::string(failed["phase"].GetString()));
  EXPECT_EQ("Failed message terminated", std::string(failed["message"].GetString()));
  EXPECT_EQ("Failed reason terminated", std::string(failed["reason"].GetString()));

  rapidjson::Document missing;
  missing.Parse(missing_res.data());
  EXPECT_EQ("Unknown", std::string(missing["phase"].GetString()));
  EXPECT_EQ("", std::string(missing["message"].GetString()));
  EXPECT_EQ("", std::string(missing["reason"].GetString()));
  EXPECT_EQ(false, missing["ready"].GetBool());
}

TEST_F(MetadataOpsTest, pod_id_to_namespace_test) {
  auto function_ctx = std::make_unique<FunctionContext>(metadata_state_, nullptr);
  auto udf_tester = px::carnot::udf::UDFTester<PodIDToNamespaceUDF>(std::move(function_ctx));
  udf_tester.ForInput("1_uid").Expect("pl");
  udf_tester.ForInput("2_uid").Expect("pl");
  udf_tester.ForInput("dne").Expect("");
}

TEST_F(MetadataOpsTest, pod_name_to_namespace_test) {
  auto function_ctx = std::make_unique<FunctionContext>(metadata_state_, nullptr);
  auto udf_tester = px::carnot::udf::UDFTester<PodNameToNamespaceUDF>(std::move(function_ctx));
  udf_tester.ForInput("pl/running_pod").Expect("pl");
  udf_tester.ForInput("px-sock-shop/terminating_pod").Expect("px-sock-shop");
  udf_tester.ForInput("badlyformed").Expect("");
}

TEST_F(MetadataOpsTest, service_name_to_namespace_test) {
  auto function_ctx = std::make_unique<FunctionContext>(metadata_state_, nullptr);
  auto udf_tester = px::carnot::udf::UDFTester<ServiceNameToNamespaceUDF>(std::move(function_ctx));
  udf_tester.ForInput("pl/orders").Expect("pl");
  udf_tester.ForInput("badlyformed").Expect("");
  udf_tester.ForInput("").Expect("");
}

TEST_F(MetadataOpsTest, has_service_id_test) {
  auto function_ctx = std::make_unique<FunctionContext>(metadata_state_, nullptr);
  auto udf_tester = px::carnot::udf::UDFTester<HasServiceIDUDF>(std::move(function_ctx));
  udf_tester.ForInput("1", "1").Expect(true);
  udf_tester.ForInput("2", "1").Expect(false);
  udf_tester.ForInput("[\"3\", \"4\"]", "4").Expect(true);
  udf_tester.ForInput("[\"3\", \"4\"]", "5").Expect(false);
  udf_tester.ForInput("[]", "4").Expect(false);
}

TEST_F(MetadataOpsTest, has_service_name_test) {
  auto function_ctx = std::make_unique<FunctionContext>(metadata_state_, nullptr);
  auto udf_tester = px::carnot::udf::UDFTester<HasServiceNameUDF>(std::move(function_ctx));
  udf_tester.ForInput("1", "1").Expect(true);
  udf_tester.ForInput("2", "1").Expect(false);
  udf_tester.ForInput("[\"3\", \"4\"]", "4").Expect(true);
  udf_tester.ForInput("[\"3\", \"4\"]", "5").Expect(false);
  udf_tester.ForInput("[]", "4").Expect(false);
}

}  // namespace metadata
}  // namespace funcs
}  // namespace carnot
}  // namespace px
