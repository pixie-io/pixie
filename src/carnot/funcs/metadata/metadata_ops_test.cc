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
#include "src/common/base/inet_utils.h"
#include "src/common/event/event.h"
#include "src/common/testing/event/simulated_time_system.h"
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
    vizier_id_ = sole::uuid4();
    time_system_ = std::make_unique<event::SimulatedTimeSystem>();
    metadata_state_ = std::make_shared<px::md::AgentMetadataState>(
        /* hostname */ "myhost",
        /* asid */ 1, /* pid */ 123, agent_id_, "mypod", vizier_id_, "myvizier",
        "myviziernamespace", time_system_.get());
    // Apply updates to metadata state.
    updates_ =
        std::make_unique<moodycamel::BlockingConcurrentQueue<std::unique_ptr<ResourceUpdate>>>();

    updates_->enqueue(px::metadatapb::testutils::CreateRunningContainerUpdatePB());
    updates_->enqueue(px::metadatapb::testutils::CreateRunningPodUpdatePB());
    updates_->enqueue(px::metadatapb::testutils::CreateRunningServiceUpdatePB());
    updates_->enqueue(px::metadatapb::testutils::CreateRunningServiceIPUpdatePB());
    updates_->enqueue(px::metadatapb::testutils::CreateRunningReplicaSetUpdatePB());
    updates_->enqueue(px::metadatapb::testutils::CreateRunningDeploymentUpdatePB());
    updates_->enqueue(px::metadatapb::testutils::CreateRunningNamespaceUpdatePB());
    updates_->enqueue(px::metadatapb::testutils::CreateTerminatingContainerUpdatePB());
    updates_->enqueue(px::metadatapb::testutils::CreateTerminatingPodUpdatePB());
    updates_->enqueue(px::metadatapb::testutils::CreateTerminatingServiceUpdatePB());
    updates_->enqueue(px::metadatapb::testutils::CreateTerminatingReplicaSetUpdatePB());
    updates_->enqueue(px::metadatapb::testutils::CreateTerminatingDeploymentUpdatePB());
    updates_->enqueue(px::metadatapb::testutils::CreateTerminatingNamespaceUpdatePB());

    auto s = px::md::ApplyK8sUpdates(10, metadata_state_.get(), &md_filter_, updates_.get());

    // Apply PID updates to metadata state.
    auto upid1 = md::UPID(123, 567, 89101);
    auto pid1 = std::make_unique<md::PIDInfo>(upid1, "exe", "test", "pod1_container_1");
    metadata_state_->AddUPID(upid1, std::move(pid1));
    auto upid2 = md::UPID(123, 567, 468);
    auto pid2 = std::make_unique<md::PIDInfo>(upid2, "exe", "cmdline", "pod2_container_1");
    metadata_state_->AddUPID(upid2, std::move(pid2));

    ASSERT_OK(s);
  }
  sole::uuid agent_id_;
  sole::uuid vizier_id_;
  std::unique_ptr<event::SimulatedTimeSystem> time_system_;
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

TEST_F(MetadataOpsTest, pod_id_to_pod_labels_test) {
  auto function_ctx = std::make_unique<FunctionContext>(metadata_state_, nullptr);
  auto udf_tester = px::carnot::udf::UDFTester<PodIDToPodLabelsUDF>(std::move(function_ctx));
  udf_tester.ForInput("1_uid").Expect("{\"k1\":\"v1\", \"k2\":\"v2\"}");
  udf_tester.ForInput("2_uid").Expect("{\"k1\":\"v1\"}");
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

TEST_F(MetadataOpsTest, upid_to_replicaset_name_test) {
  auto function_ctx = std::make_unique<FunctionContext>(metadata_state_, nullptr);
  auto udf_tester = px::carnot::udf::UDFTester<UPIDToReplicaSetNameUDF>(std::move(function_ctx));
  auto upid1 = types::UInt128Value(528280977975, 89101);
  udf_tester.ForInput(upid1).Expect("pl/rs0");
  auto upid2 = types::UInt128Value(528280977975, 468);
  udf_tester.ForInput(upid2).Expect("pl/terminating_rs");
  auto upid3 = types::UInt128Value(528280977975, 123);
  udf_tester.ForInput(upid3).Expect("");

  updates_->enqueue(px::metadatapb::testutils::CreateTerminatedReplicaSetUpdatePB());
  EXPECT_OK(px::md::ApplyK8sUpdates(11, metadata_state_.get(), &md_filter_, updates_.get()));
  // upid2 previously was connected to pl/terminating_service.
  udf_tester.ForInput(upid2).Expect("");
}

TEST_F(MetadataOpsTest, upid_to_replicaset_id_test) {
  auto function_ctx = std::make_unique<FunctionContext>(metadata_state_, nullptr);
  auto udf_tester = px::carnot::udf::UDFTester<UPIDToReplicaSetIDUDF>(std::move(function_ctx));
  auto upid1 = types::UInt128Value(528280977975, 89101);
  udf_tester.ForInput(upid1).Expect("rs0_uid");
  auto upid2 = types::UInt128Value(528280977975, 468);
  udf_tester.ForInput(upid2).Expect("terminating_rs0_uid");
  auto upid3 = types::UInt128Value(528280977975, 123);
  udf_tester.ForInput(upid3).Expect("");

  // Terminate a replica set, and make sure that the upid no longer associates with that replica
  // set.
  updates_->enqueue(px::metadatapb::testutils::CreateTerminatedReplicaSetUpdatePB());
  EXPECT_OK(px::md::ApplyK8sUpdates(11, metadata_state_.get(), &md_filter_, updates_.get()));
  // upid2 previously was connected to 4_uid.
  udf_tester.ForInput(upid2).Expect("");
}

TEST_F(MetadataOpsTest, upid_to_deployment_name_test) {
  auto function_ctx = std::make_unique<FunctionContext>(metadata_state_, nullptr);
  auto udf_tester = px::carnot::udf::UDFTester<UPIDToDeploymentNameUDF>(std::move(function_ctx));
  auto upid1 = types::UInt128Value(528280977975, 89101);
  udf_tester.ForInput(upid1).Expect("pl/deployment1");
  auto upid2 = types::UInt128Value(528280977975, 468);
  udf_tester.ForInput(upid2).Expect("pl/terminating_deployment1");
  auto upid3 = types::UInt128Value(528280977975, 123);
  udf_tester.ForInput(upid3).Expect("");

  updates_->enqueue(px::metadatapb::testutils::CreateTerminatedDeploymentUpdatePB());
  EXPECT_OK(px::md::ApplyK8sUpdates(11, metadata_state_.get(), &md_filter_, updates_.get()));
  // upid2 previously was connected to pl/terminating_service.
  udf_tester.ForInput(upid2).Expect("");
}

TEST_F(MetadataOpsTest, upid_to_deployment_id_test) {
  auto function_ctx = std::make_unique<FunctionContext>(metadata_state_, nullptr);
  auto udf_tester = px::carnot::udf::UDFTester<UPIDToDeploymentIDUDF>(std::move(function_ctx));
  auto upid1 = types::UInt128Value(528280977975, 89101);
  udf_tester.ForInput(upid1).Expect("deployment_uid");
  auto upid2 = types::UInt128Value(528280977975, 468);
  udf_tester.ForInput(upid2).Expect("terminating_deployment_uid");
  auto upid3 = types::UInt128Value(528280977975, 123);
  udf_tester.ForInput(upid3).Expect("");

  // Terminate a replica set, and make sure that the upid no longer associates with that replica
  // set.
  updates_->enqueue(px::metadatapb::testutils::CreateTerminatedDeploymentUpdatePB());
  EXPECT_OK(px::md::ApplyK8sUpdates(11, metadata_state_.get(), &md_filter_, updates_.get()));
  // upid2 previously was connected to 4_uid.
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

TEST_F(MetadataOpsTest, pod_id_to_replicaset_name_test) {
  auto function_ctx = std::make_unique<FunctionContext>(metadata_state_, nullptr);
  auto udf_tester = px::carnot::udf::UDFTester<PodIDToReplicaSetNameUDF>(std::move(function_ctx));
  udf_tester.ForInput("1_uid").Expect("pl/rs0");
  // This pod is not available, should return empty.
  udf_tester.ForInput("123_uid").Expect("");
}

TEST_F(MetadataOpsTest, pod_id_to_replicaset_id_test) {
  auto function_ctx = std::make_unique<FunctionContext>(metadata_state_, nullptr);
  auto udf_tester = px::carnot::udf::UDFTester<PodIDToReplicaSetIDUDF>(std::move(function_ctx));
  udf_tester.ForInput("1_uid").Expect("rs0_uid");
  // This pod is not available, should return empty.
  udf_tester.ForInput("123_uid").Expect("");
}

TEST_F(MetadataOpsTest, pod_id_to_deployment_name_test) {
  auto function_ctx = std::make_unique<FunctionContext>(metadata_state_, nullptr);
  auto udf_tester = px::carnot::udf::UDFTester<PodIDToDeploymentNameUDF>(std::move(function_ctx));
  udf_tester.ForInput("1_uid").Expect("pl/deployment1");
  udf_tester.ForInput("2_uid").Expect("pl/terminating_deployment1");
  // This pod is not available, should return empty.
  udf_tester.ForInput("123_uid").Expect("");

  // keep information about the owners after termination
  updates_->enqueue(px::metadatapb::testutils::CreateTerminatedPodUpdatePB());
  updates_->enqueue(px::metadatapb::testutils::CreateTerminatedReplicaSetUpdatePB());
  updates_->enqueue(px::metadatapb::testutils::CreateTerminatedDeploymentUpdatePB());
  EXPECT_OK(px::md::ApplyK8sUpdates(11, metadata_state_.get(), &md_filter_, updates_.get()));
  udf_tester.ForInput("2_uid").Expect("pl/terminating_deployment1");
}

TEST_F(MetadataOpsTest, pod_id_to_deployment_id_test) {
  auto function_ctx = std::make_unique<FunctionContext>(metadata_state_, nullptr);
  auto udf_tester = px::carnot::udf::UDFTester<PodIDToDeploymentIDUDF>(std::move(function_ctx));
  udf_tester.ForInput("1_uid").Expect("deployment_uid");
  udf_tester.ForInput("2_uid").Expect("terminating_deployment_uid");
  // This pod is not available, should return empty.
  udf_tester.ForInput("123_uid").Expect("");

  // keep information about the deployment after termination
  updates_->enqueue(px::metadatapb::testutils::CreateTerminatedPodUpdatePB());
  updates_->enqueue(px::metadatapb::testutils::CreateTerminatedReplicaSetUpdatePB());
  updates_->enqueue(px::metadatapb::testutils::CreateTerminatedDeploymentUpdatePB());
  EXPECT_OK(px::md::ApplyK8sUpdates(11, metadata_state_.get(), &md_filter_, updates_.get()));
  udf_tester.ForInput("2_uid").Expect("terminating_deployment_uid");
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

TEST_F(MetadataOpsTest, service_id_to_cluster_ip_test) {
  auto function_ctx = std::make_unique<FunctionContext>(metadata_state_, nullptr);
  auto udf_tester = px::carnot::udf::UDFTester<ServiceIDToClusterIPUDF>(std::move(function_ctx));
  udf_tester.ForInput("3_uid").Expect("127.0.0.2");
  udf_tester.ForInput("4_uid").Expect("");
}

TEST_F(MetadataOpsTest, service_id_to_external_ips_test) {
  auto function_ctx = std::make_unique<FunctionContext>(metadata_state_, nullptr);
  auto udf_tester = px::carnot::udf::UDFTester<ServiceIDToExternalIPsUDF>(std::move(function_ctx));
  udf_tester.ForInput("3_uid").Expect("[\"127.0.0.1\"]");
  udf_tester.ForInput("4_uid").Expect("[]");
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

TEST_F(MetadataOpsTest, pod_id_to_owner_references_test) {
  auto function_ctx = std::make_unique<FunctionContext>(metadata_state_, nullptr);
  auto udf_tester = px::carnot::udf::UDFTester<PodIDToOwnerReferencesUDF>(std::move(function_ctx));
  udf_tester.ForInput("1_uid").Expect(
      R"(["{\"uid\":\"rs0_uid\",\"kind\":\"ReplicaSet\",\"name\":\"rs0\"}"])");
  // This pod is not available, should return empty.
  udf_tester.ForInput("123_uid").Expect("");
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

TEST_F(MetadataOpsTest, upid_to_start_ts) {
  UPIDToStartTSUDF udf;
  auto upid1 = md::UPID(123, 567, 89101);
  EXPECT_EQ(udf.Exec(nullptr, upid1.value()), 89101);

  auto upid2 = md::UPID(255, 123, 11111);
  EXPECT_EQ(udf.Exec(nullptr, upid2.value()), 11111);
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

TEST_F(MetadataOpsTest, pod_name_to_owner_references_test) {
  auto function_ctx = std::make_unique<FunctionContext>(metadata_state_, nullptr);
  auto udf_tester =
      px::carnot::udf::UDFTester<PodNameToOwnerReferencesUDF>(std::move(function_ctx));
  udf_tester.ForInput("pl/running_pod")
      .Expect(R"(["{\"uid\":\"rs0_uid\",\"kind\":\"ReplicaSet\",\"name\":\"rs0\"}"])");
  udf_tester.ForInput("pl/terminating_pod")
      .Expect(
          R"(["{\"uid\":\"terminating_rs0_uid\",\"kind\":\"ReplicaSet\",\"name\":\"terminating_rs\"}"])");
  udf_tester.ForInput("badlyformed").Expect("");
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

TEST_F(MetadataOpsTest, ip_to_pod_id_test) {
  auto function_ctx = std::make_unique<FunctionContext>(metadata_state_, nullptr);

  IPToPodIDUDF udf;
  EXPECT_EQ(udf.Exec(function_ctx.get(), "1.1.1.1"), "1_uid");
  EXPECT_EQ(udf.Exec(function_ctx.get(), "1.2.1.2"), "");
  EXPECT_EQ(udf.Exec(function_ctx.get(), "127.0.0.2"), "");
}

TEST_F(MetadataOpsTest, ip_to_pod_id_at_time_test) {
  auto function_ctx = std::make_unique<FunctionContext>(metadata_state_, nullptr);

  IPToPodIDAtTimeUDF udf;
  EXPECT_EQ(udf.Exec(function_ctx.get(), "1.1.1.1", 4), "");
  EXPECT_EQ(udf.Exec(function_ctx.get(), "1.1.1.1", 8), "1_uid");
  EXPECT_EQ(udf.Exec(function_ctx.get(), "1.1.1.1", 100), "1_uid");
  EXPECT_EQ(udf.Exec(function_ctx.get(), "1.2.1.2", 4), "");

  updates_->enqueue(px::metadatapb::testutils::CreateReusedIPUpdatePB());
  EXPECT_OK(px::md::ApplyK8sUpdates(11, metadata_state_.get(), &md_filter_, updates_.get()));

  EXPECT_EQ(udf.Exec(function_ctx.get(), "1.1.1.1", 4), "");
  EXPECT_EQ(udf.Exec(function_ctx.get(), "1.1.1.1", 8), "1_uid");
  // Now a new pod 101_uid is using this IP address and has a start_time of 100
  EXPECT_EQ(udf.Exec(function_ctx.get(), "1.1.1.1", 100), "101_uid");
  EXPECT_EQ(udf.Exec(function_ctx.get(), "1.2.1.2", 4), "");
}

TEST_F(MetadataOpsTest, ip_to_service_id_test) {
  auto function_ctx = std::make_unique<FunctionContext>(metadata_state_, nullptr);
  IPToServiceIDUDF udf;
  EXPECT_EQ(udf.Exec(function_ctx.get(), "1.1.1.1"), "3_uid");
  EXPECT_EQ(udf.Exec(function_ctx.get(), "1.1.1.10"), "");
  EXPECT_EQ(udf.Exec(function_ctx.get(), "1.2.1.2"), "");
  EXPECT_EQ(udf.Exec(function_ctx.get(), "127.0.0.2"), "3_uid");
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

TEST_F(MetadataOpsTest, pod_name_to_replicaset_name_test) {
  auto function_ctx = std::make_unique<FunctionContext>(metadata_state_, nullptr);
  auto udf_tester = px::carnot::udf::UDFTester<PodNameToReplicaSetNameUDF>(std::move(function_ctx));
  udf_tester.ForInput("pl/running_pod").Expect("pl/rs0");
  udf_tester.ForInput("pl/terminating_pod").Expect("pl/terminating_rs");
  udf_tester.ForInput("badlyformed").Expect("");
}

TEST_F(MetadataOpsTest, pod_name_to_replicaset_id_test) {
  updates_->enqueue(px::metadatapb::testutils::CreateMissingOwnerPodUpdatePB());
  EXPECT_OK(px::md::ApplyK8sUpdates(11, metadata_state_.get(), &md_filter_, updates_.get()));
  auto function_ctx = std::make_unique<FunctionContext>(metadata_state_, nullptr);
  auto udf_tester = px::carnot::udf::UDFTester<PodNameToReplicaSetIDUDF>(std::move(function_ctx));
  udf_tester.ForInput("pl/running_pod").Expect("rs0_uid");
  udf_tester.ForInput("pl/terminating_pod").Expect("terminating_rs0_uid");
  udf_tester.ForInput("badlyformed").Expect("");

  // The owner is not available, should return empty.
  udf_tester.ForInput("pl/missing_owner_pod").Expect("");
}

TEST_F(MetadataOpsTest, pod_name_to_deployment_name_test) {
  auto function_ctx = std::make_unique<FunctionContext>(metadata_state_, nullptr);
  auto udf_tester = px::carnot::udf::UDFTester<PodNameToDeploymentNameUDF>(std::move(function_ctx));
  udf_tester.ForInput("pl/running_pod").Expect("pl/deployment1");
  udf_tester.ForInput("pl/terminating_pod").Expect("pl/terminating_deployment1");
  udf_tester.ForInput("badlyformed").Expect("");

  // keep information about the deployment after termination
  updates_->enqueue(px::metadatapb::testutils::CreateTerminatedPodUpdatePB());
  updates_->enqueue(px::metadatapb::testutils::CreateTerminatedReplicaSetUpdatePB());
  updates_->enqueue(px::metadatapb::testutils::CreateTerminatedDeploymentUpdatePB());
  EXPECT_OK(px::md::ApplyK8sUpdates(11, metadata_state_.get(), &md_filter_, updates_.get()));
  udf_tester.ForInput("pl/terminating_pod").Expect("pl/terminating_deployment1");
}

TEST_F(MetadataOpsTest, pod_name_to_deployment_id_test) {
  auto function_ctx = std::make_unique<FunctionContext>(metadata_state_, nullptr);
  auto udf_tester = px::carnot::udf::UDFTester<PodNameToDeploymentIDUDF>(std::move(function_ctx));
  udf_tester.ForInput("pl/running_pod").Expect("deployment_uid");
  udf_tester.ForInput("pl/terminating_pod").Expect("terminating_deployment_uid");
  udf_tester.ForInput("badlyformed").Expect("");
  // keep information about the deployment after termination
  updates_->enqueue(px::metadatapb::testutils::CreateTerminatedPodUpdatePB());
  updates_->enqueue(px::metadatapb::testutils::CreateTerminatedReplicaSetUpdatePB());
  updates_->enqueue(px::metadatapb::testutils::CreateTerminatedDeploymentUpdatePB());
  EXPECT_OK(px::md::ApplyK8sUpdates(11, metadata_state_.get(), &md_filter_, updates_.get()));
  udf_tester.ForInput("pl/terminating_pod").Expect("terminating_deployment_uid");
}

TEST_F(MetadataOpsTest, service_name_to_namespace_test) {
  auto function_ctx = std::make_unique<FunctionContext>(metadata_state_, nullptr);
  auto udf_tester = px::carnot::udf::UDFTester<ServiceNameToNamespaceUDF>(std::move(function_ctx));
  udf_tester.ForInput("pl/orders").Expect("pl");
  udf_tester.ForInput("badlyformed").Expect("");
  udf_tester.ForInput("").Expect("");
}

TEST_F(MetadataOpsTest, replicaset_id_to_replicaset_name_test) {
  auto function_ctx = std::make_unique<FunctionContext>(metadata_state_, nullptr);
  auto udf_tester =
      px::carnot::udf::UDFTester<ReplicaSetIDToReplicaSetNameUDF>(std::move(function_ctx));
  udf_tester.ForInput("rs0_uid").Expect("pl/rs0");
  udf_tester.ForInput("terminating_rs0_uid").Expect("pl/terminating_rs");
  udf_tester.ForInput("badformat").Expect("");
  updates_->enqueue(px::metadatapb::testutils::CreateTerminatedReplicaSetUpdatePB());
  EXPECT_OK(px::md::ApplyK8sUpdates(11, metadata_state_.get(), &md_filter_, updates_.get()));
  // upid2 previously was connected to 4_uid.
  udf_tester.ForInput("terminating_rs0_uid").Expect("pl/terminating_rs");
}

TEST_F(MetadataOpsTest, replicaset_id_to_start_time_test) {
  auto function_ctx = std::make_unique<FunctionContext>(metadata_state_, nullptr);
  auto udf_tester = px::carnot::udf::UDFTester<ReplicaSetIDToStartTimeUDF>(std::move(function_ctx));
  udf_tester.ForInput("rs0_uid").Expect(101);
  udf_tester.ForInput("terminating_rs0_uid").Expect(101);
  udf_tester.ForInput("badformat").Expect(0);
  updates_->enqueue(px::metadatapb::testutils::CreateTerminatedReplicaSetUpdatePB());
  EXPECT_OK(px::md::ApplyK8sUpdates(11, metadata_state_.get(), &md_filter_, updates_.get()));
  // upid2 previously was connected to 4_uid.
  udf_tester.ForInput("terminating_rs0_uid").Expect(101);
}

TEST_F(MetadataOpsTest, replicaset_id_to_stop_time_test) {
  auto function_ctx = std::make_unique<FunctionContext>(metadata_state_, nullptr);
  auto udf_tester = px::carnot::udf::UDFTester<ReplicaSetIDToStopTimeUDF>(std::move(function_ctx));
  udf_tester.ForInput("rs0_uid").Expect(0);
  udf_tester.ForInput("terminating_rs0_uid").Expect(0);
  udf_tester.ForInput("badformat").Expect(0);
  updates_->enqueue(px::metadatapb::testutils::CreateTerminatedReplicaSetUpdatePB());
  EXPECT_OK(px::md::ApplyK8sUpdates(11, metadata_state_.get(), &md_filter_, updates_.get()));
  // upid2 previously was connected to 4_uid.
  udf_tester.ForInput("terminating_rs0_uid").Expect(150);
}

TEST_F(MetadataOpsTest, replicaset_id_to_namespace_test) {
  auto function_ctx = std::make_unique<FunctionContext>(metadata_state_, nullptr);
  auto udf_tester = px::carnot::udf::UDFTester<ReplicaSetIDToNamespaceUDF>(std::move(function_ctx));
  udf_tester.ForInput("rs0_uid").Expect("pl");
  udf_tester.ForInput("terminating_rs0_uid").Expect("pl");
  udf_tester.ForInput("badformat").Expect("");
  updates_->enqueue(px::metadatapb::testutils::CreateTerminatedReplicaSetUpdatePB());
  EXPECT_OK(px::md::ApplyK8sUpdates(11, metadata_state_.get(), &md_filter_, updates_.get()));
  // upid2 previously was connected to 4_uid.
  udf_tester.ForInput("terminating_rs0_uid").Expect("pl");
}

TEST_F(MetadataOpsTest, replicaset_id_to_owner_references_test) {
  auto function_ctx = std::make_unique<FunctionContext>(metadata_state_, nullptr);
  auto udf_tester =
      px::carnot::udf::UDFTester<ReplicaSetIDToOwnerReferencesUDF>(std::move(function_ctx));

  udf_tester.ForInput("rs0_uid").Expect(
      R"(["{\"uid\":\"deployment_uid\",\"kind\":\"Deployment\",\"name\":\"deployment1\"}"])");
  udf_tester.ForInput("terminating_rs0_uid")
      .Expect(
          R"(["{\"uid\":\"terminating_deployment_uid\",\"kind\":\"Deployment\",\"name\":\"terminating_deployment1\"}"])");
  udf_tester.ForInput("badformat").Expect("");
  updates_->enqueue(px::metadatapb::testutils::CreateTerminatedReplicaSetUpdatePB());
  EXPECT_OK(px::md::ApplyK8sUpdates(11, metadata_state_.get(), &md_filter_, updates_.get()));
  // upid2 previously was connected to 4_uid.
  udf_tester.ForInput("terminating_rs0_uid")
      .Expect(
          R"(["{\"uid\":\"terminating_deployment_uid\",\"kind\":\"Deployment\",\"name\":\"terminating_deployment1\"}"])");
}

TEST_F(MetadataOpsTest, replicaset_id_to_status_test) {
  auto function_ctx = std::make_unique<FunctionContext>(metadata_state_, nullptr);
  auto udf_tester = px::carnot::udf::UDFTester<ReplicaSetIDToStatusUDF>(std::move(function_ctx));

  md::ReplicaSetConditions conditions_ready{{"ready", md::ConditionStatus::kTrue}};
  md::ReplicaSetConditions conditions_terminated{{"Terminating", md::ConditionStatus::kTrue}};
  md::ReplicaSetInfo rs_info_ready("", "", "", 5, 5, 3, 3, 5, 5, conditions_ready, 101, 0);
  md::ReplicaSetInfo rs_info_terminating("", "", "", 0, 0, 0, 0, 5, 0, conditions_terminated, 101,
                                         0);

  udf_tester.ForInput("rs0_uid").Expect(ReplicaSetInfoToStatus(&rs_info_ready));
  udf_tester.ForInput("terminating_rs0_uid").Expect(ReplicaSetInfoToStatus(&rs_info_terminating));
  udf_tester.ForInput("badformat").Expect("");
  updates_->enqueue(px::metadatapb::testutils::CreateTerminatedReplicaSetUpdatePB());
  EXPECT_OK(px::md::ApplyK8sUpdates(11, metadata_state_.get(), &md_filter_, updates_.get()));
  // upid2 previously was connected to 4_uid.
  udf_tester.ForInput("terminating_rs0_uid").Expect(ReplicaSetInfoToStatus(&rs_info_terminating));
}

TEST_F(MetadataOpsTest, replicaset_id_to_deployment_name_test) {
  auto function_ctx = std::make_unique<FunctionContext>(metadata_state_, nullptr);
  auto udf_tester =
      px::carnot::udf::UDFTester<ReplicaSetIDToDeploymentNameUDF>(std::move(function_ctx));
  udf_tester.ForInput("rs0_uid").Expect("pl/deployment1");
  udf_tester.ForInput("terminating_rs0_uid").Expect("pl/terminating_deployment1");
  udf_tester.ForInput("badlyformed").Expect("");
  // keep information about the deployment after termination
  updates_->enqueue(px::metadatapb::testutils::CreateTerminatedReplicaSetUpdatePB());
  updates_->enqueue(px::metadatapb::testutils::CreateTerminatedDeploymentUpdatePB());
  EXPECT_OK(px::md::ApplyK8sUpdates(11, metadata_state_.get(), &md_filter_, updates_.get()));
  udf_tester.ForInput("terminating_rs0_uid").Expect("pl/terminating_deployment1");
}

TEST_F(MetadataOpsTest, replicaset_id_to_deployment_id_test) {
  auto function_ctx = std::make_unique<FunctionContext>(metadata_state_, nullptr);
  auto udf_tester =
      px::carnot::udf::UDFTester<ReplicaSetIDToDeploymentIDUDF>(std::move(function_ctx));
  udf_tester.ForInput("rs0_uid").Expect("deployment_uid");
  udf_tester.ForInput("terminating_rs0_uid").Expect("terminating_deployment_uid");
  udf_tester.ForInput("badlyformed").Expect("");
  // keep information about the deployment after termination
  updates_->enqueue(px::metadatapb::testutils::CreateTerminatedReplicaSetUpdatePB());
  updates_->enqueue(px::metadatapb::testutils::CreateTerminatedDeploymentUpdatePB());
  EXPECT_OK(px::md::ApplyK8sUpdates(11, metadata_state_.get(), &md_filter_, updates_.get()));
  udf_tester.ForInput("terminating_rs0_uid").Expect("terminating_deployment_uid");
}

TEST_F(MetadataOpsTest, replicaset_name_to_replicaset_id_test) {
  auto function_ctx = std::make_unique<FunctionContext>(metadata_state_, nullptr);
  auto udf_tester =
      px::carnot::udf::UDFTester<ReplicaSetNameToReplicaSetIDUDF>(std::move(function_ctx));
  udf_tester.ForInput("pl/rs0").Expect("rs0_uid");
  udf_tester.ForInput("pl/terminating_rs").Expect("terminating_rs0_uid");
  udf_tester.ForInput("badformat").Expect("");
  updates_->enqueue(px::metadatapb::testutils::CreateTerminatedReplicaSetUpdatePB());
  // keep information about the replica set after termination
  EXPECT_OK(px::md::ApplyK8sUpdates(11, metadata_state_.get(), &md_filter_, updates_.get()));
  udf_tester.ForInput("pl/terminating_rs").Expect("terminating_rs0_uid");
}

TEST_F(MetadataOpsTest, replicaset_name_to_start_time_test) {
  auto function_ctx = std::make_unique<FunctionContext>(metadata_state_, nullptr);
  auto udf_tester =
      px::carnot::udf::UDFTester<ReplicaSetNameToStartTimeUDF>(std::move(function_ctx));
  udf_tester.ForInput("pl/rs0").Expect(101);
  udf_tester.ForInput("pl/terminating_rs").Expect(101);
  udf_tester.ForInput("badformat").Expect(0);
  updates_->enqueue(px::metadatapb::testutils::CreateTerminatedReplicaSetUpdatePB());
  // keep information about the replica set after termination
  EXPECT_OK(px::md::ApplyK8sUpdates(11, metadata_state_.get(), &md_filter_, updates_.get()));
  udf_tester.ForInput("pl/terminating_rs").Expect(101);
}

TEST_F(MetadataOpsTest, replicaset_name_to_stop_time_test) {
  auto function_ctx = std::make_unique<FunctionContext>(metadata_state_, nullptr);
  auto udf_tester =
      px::carnot::udf::UDFTester<ReplicaSetNameToStopTimeUDF>(std::move(function_ctx));
  udf_tester.ForInput("pl/rs0").Expect(0);
  udf_tester.ForInput("pl/terminating_rs").Expect(0);
  udf_tester.ForInput("badformat").Expect(0);
  updates_->enqueue(px::metadatapb::testutils::CreateTerminatedReplicaSetUpdatePB());
  // keep information about the replica set after termination
  EXPECT_OK(px::md::ApplyK8sUpdates(11, metadata_state_.get(), &md_filter_, updates_.get()));
  udf_tester.ForInput("pl/terminating_rs").Expect(150);
}

TEST_F(MetadataOpsTest, replicaset_name_to_namespace_test) {
  auto function_ctx = std::make_unique<FunctionContext>(metadata_state_, nullptr);
  auto udf_tester =
      px::carnot::udf::UDFTester<ReplicaSetNameToNamespaceUDF>(std::move(function_ctx));
  udf_tester.ForInput("pl/rs0").Expect("pl");
  udf_tester.ForInput("pl/terminating_rs").Expect("pl");
  udf_tester.ForInput("badformat").Expect("");
  updates_->enqueue(px::metadatapb::testutils::CreateTerminatedReplicaSetUpdatePB());
  // keep information about the replica set after termination
  EXPECT_OK(px::md::ApplyK8sUpdates(11, metadata_state_.get(), &md_filter_, updates_.get()));
  udf_tester.ForInput("pl/terminating_rs").Expect("pl");
}

TEST_F(MetadataOpsTest, replicaset_name_to_owner_references_test) {
  auto function_ctx = std::make_unique<FunctionContext>(metadata_state_, nullptr);
  auto udf_tester =
      px::carnot::udf::UDFTester<ReplicaSetNameToOwnerReferencesUDF>(std::move(function_ctx));

  px::md::OwnerReference ref{"deployment_uid", "deployment1", "Deployment"};

  udf_tester.ForInput("pl/rs0").Expect(
      R"(["{\"uid\":\"deployment_uid\",\"kind\":\"Deployment\",\"name\":\"deployment1\"}"])");
  udf_tester.ForInput("pl/terminating_rs")
      .Expect(
          R"(["{\"uid\":\"terminating_deployment_uid\",\"kind\":\"Deployment\",\"name\":\"terminating_deployment1\"}"])");
  udf_tester.ForInput("badformat").Expect("");
  updates_->enqueue(px::metadatapb::testutils::CreateTerminatedReplicaSetUpdatePB());
  // keep information about the replica set after termination
  EXPECT_OK(px::md::ApplyK8sUpdates(11, metadata_state_.get(), &md_filter_, updates_.get()));
  udf_tester.ForInput("pl/terminating_rs")
      .Expect(
          R"(["{\"uid\":\"terminating_deployment_uid\",\"kind\":\"Deployment\",\"name\":\"terminating_deployment1\"}"])");
}

TEST_F(MetadataOpsTest, replicaset_name_to_status_test) {
  auto function_ctx = std::make_unique<FunctionContext>(metadata_state_, nullptr);
  auto udf_tester = px::carnot::udf::UDFTester<ReplicaSetNameToStatusUDF>(std::move(function_ctx));

  md::ReplicaSetConditions conditionsReady{{"ready", md::ConditionStatus::kTrue}};
  md::ReplicaSetConditions conditionsTerminating{{"Terminating", md::ConditionStatus::kTrue}};
  md::ReplicaSetInfo rs_info_ready("", "", "", 5, 5, 3, 3, 5, 5, conditionsReady, 101, 0);
  md::ReplicaSetInfo rs_info_terminating("", "", "", 0, 0, 0, 0, 5, 0, conditionsTerminating, 101,
                                         0);

  udf_tester.ForInput("pl/rs0").Expect(ReplicaSetInfoToStatus(&rs_info_ready));
  udf_tester.ForInput("pl/terminating_rs").Expect(ReplicaSetInfoToStatus(&rs_info_terminating));
  udf_tester.ForInput("badformat").Expect("");
  updates_->enqueue(px::metadatapb::testutils::CreateTerminatedReplicaSetUpdatePB());
  // keep information about the replica set after termination
  EXPECT_OK(px::md::ApplyK8sUpdates(11, metadata_state_.get(), &md_filter_, updates_.get()));
  udf_tester.ForInput("pl/terminating_rs").Expect(ReplicaSetInfoToStatus(&rs_info_terminating));
}

TEST_F(MetadataOpsTest, replicaset_name_to_deployment_name_test) {
  auto function_ctx = std::make_unique<FunctionContext>(metadata_state_, nullptr);
  auto udf_tester =
      px::carnot::udf::UDFTester<ReplicaSetNameToDeploymentNameUDF>(std::move(function_ctx));
  udf_tester.ForInput("pl/rs0").Expect("pl/deployment1");
  udf_tester.ForInput("pl/terminating_rs").Expect("pl/terminating_deployment1");
  udf_tester.ForInput("badlyformed").Expect("");
  // keep information about the deployment after termination
  updates_->enqueue(px::metadatapb::testutils::CreateTerminatedReplicaSetUpdatePB());
  updates_->enqueue(px::metadatapb::testutils::CreateTerminatedDeploymentUpdatePB());
  EXPECT_OK(px::md::ApplyK8sUpdates(11, metadata_state_.get(), &md_filter_, updates_.get()));
  udf_tester.ForInput("pl/terminating_rs").Expect("pl/terminating_deployment1");
}

TEST_F(MetadataOpsTest, replicaset_name_to_deployment_id_test) {
  auto function_ctx = std::make_unique<FunctionContext>(metadata_state_, nullptr);
  auto udf_tester =
      px::carnot::udf::UDFTester<ReplicaSetNameToDeploymentIDUDF>(std::move(function_ctx));
  udf_tester.ForInput("pl/rs0").Expect("deployment_uid");
  udf_tester.ForInput("pl/terminating_rs").Expect("terminating_deployment_uid");
  udf_tester.ForInput("badlyformed").Expect("");
  // keep information about the deployment after termination
  updates_->enqueue(px::metadatapb::testutils::CreateTerminatedReplicaSetUpdatePB());
  updates_->enqueue(px::metadatapb::testutils::CreateTerminatedDeploymentUpdatePB());
  EXPECT_OK(px::md::ApplyK8sUpdates(11, metadata_state_.get(), &md_filter_, updates_.get()));
  udf_tester.ForInput("pl/terminating_rs").Expect("terminating_deployment_uid");
}

TEST_F(MetadataOpsTest, deployment_id_to_deployment_name_test) {
  auto function_ctx = std::make_unique<FunctionContext>(metadata_state_, nullptr);
  auto udf_tester =
      px::carnot::udf::UDFTester<DeploymentIDToDeploymentNameUDF>(std::move(function_ctx));
  udf_tester.ForInput("deployment_uid").Expect("pl/deployment1");
  udf_tester.ForInput("terminating_deployment_uid").Expect("pl/terminating_deployment1");
  udf_tester.ForInput("badformat").Expect("");
  updates_->enqueue(px::metadatapb::testutils::CreateTerminatedDeploymentUpdatePB());
  EXPECT_OK(px::md::ApplyK8sUpdates(11, metadata_state_.get(), &md_filter_, updates_.get()));
  // make sure that we can still find the name of terminated deployment from uid
  udf_tester.ForInput("terminating_deployment_uid").Expect("pl/terminating_deployment1");
}

TEST_F(MetadataOpsTest, deployment_id_to_start_time_test) {
  auto function_ctx = std::make_unique<FunctionContext>(metadata_state_, nullptr);
  auto udf_tester = px::carnot::udf::UDFTester<DeploymentIDToStartTimeUDF>(std::move(function_ctx));
  udf_tester.ForInput("deployment_uid").Expect(101);
  udf_tester.ForInput("terminating_deployment_uid").Expect(123);
  udf_tester.ForInput("badformat").Expect(0);
  updates_->enqueue(px::metadatapb::testutils::CreateTerminatedDeploymentUpdatePB());
  EXPECT_OK(px::md::ApplyK8sUpdates(11, metadata_state_.get(), &md_filter_, updates_.get()));
  // start time shouldn't change after pod terminated
  udf_tester.ForInput("terminating_deployment_uid").Expect(123);
}

TEST_F(MetadataOpsTest, deployment_id_to_stop_time_test) {
  auto function_ctx = std::make_unique<FunctionContext>(metadata_state_, nullptr);
  auto udf_tester = px::carnot::udf::UDFTester<DeploymentIDToStopTimeUDF>(std::move(function_ctx));
  udf_tester.ForInput("deployment_uid").Expect(0);
  udf_tester.ForInput("terminating_deployment_uid").Expect(0);
  udf_tester.ForInput("badformat").Expect(0);
  updates_->enqueue(px::metadatapb::testutils::CreateTerminatedDeploymentUpdatePB());
  EXPECT_OK(px::md::ApplyK8sUpdates(11, metadata_state_.get(), &md_filter_, updates_.get()));
  // deployment should change stop time on deployment termination
  udf_tester.ForInput("terminating_deployment_uid").Expect(150);
}

TEST_F(MetadataOpsTest, deployment_id_to_namespace_test) {
  auto function_ctx = std::make_unique<FunctionContext>(metadata_state_, nullptr);
  auto udf_tester = px::carnot::udf::UDFTester<DeploymentIDToNamespaceUDF>(std::move(function_ctx));
  udf_tester.ForInput("deployment_uid").Expect("pl");
  udf_tester.ForInput("terminating_deployment_uid").Expect("pl");
  udf_tester.ForInput("badformat").Expect("");
  updates_->enqueue(px::metadatapb::testutils::CreateTerminatedDeploymentUpdatePB());
  EXPECT_OK(px::md::ApplyK8sUpdates(11, metadata_state_.get(), &md_filter_, updates_.get()));
  // upid2 previously was connected to 4_uid.
  udf_tester.ForInput("terminating_deployment_uid").Expect("pl");
}

TEST_F(MetadataOpsTest, deployment_id_to_status_test) {
  auto function_ctx = std::make_unique<FunctionContext>(metadata_state_, nullptr);
  auto udf_tester = px::carnot::udf::UDFTester<DeploymentIDToStatusUDF>(std::move(function_ctx));

  // can't test with more conditions since conditions are stored in unordered map
  // with this test suite, condition order changes, so test fail if more than one
  // condition is given
  md::DeploymentConditions conditions_ready{
      {md::DeploymentConditionType::kAvailable, md::ConditionStatus::kTrue}};

  md::DeploymentConditions conditions_terminating{
      {md::DeploymentConditionType::kAvailable, md::ConditionStatus::kFalse}};

  md::DeploymentInfo dep_info_ready("", "", "", 5, 5, 4, 3, 3, 2, 5, conditions_ready, 101, 0);
  md::DeploymentInfo dep_info_terminating("", "", "", 2, 6, 5, 3, 3, 2, 0, conditions_terminating,
                                          123, 0);
  md::DeploymentInfo dep_info_terminated("", "", "", 2, 0, 0, 0, 0, 0, 0, conditions_terminating,
                                         123, 150);

  udf_tester.ForInput("deployment_uid").Expect(DeploymentInfoToStatus(&dep_info_ready));
  udf_tester.ForInput("terminating_deployment_uid")
      .Expect(DeploymentInfoToStatus(&dep_info_terminating));
  udf_tester.ForInput("badformat").Expect("");
  updates_->enqueue(px::metadatapb::testutils::CreateTerminatedDeploymentUpdatePB());
  EXPECT_OK(px::md::ApplyK8sUpdates(11, metadata_state_.get(), &md_filter_, updates_.get()));
  // check status after deployment terminated
  udf_tester.ForInput("terminating_deployment_uid")
      .Expect(DeploymentInfoToStatus(&dep_info_terminated));
}

TEST_F(MetadataOpsTest, deployment_name_to_deployment_id_test) {
  auto function_ctx = std::make_unique<FunctionContext>(metadata_state_, nullptr);
  auto udf_tester =
      px::carnot::udf::UDFTester<DeploymentNameToDeploymentIDUDF>(std::move(function_ctx));
  udf_tester.ForInput("pl/deployment1").Expect("deployment_uid");
  udf_tester.ForInput("pl/terminating_deployment1").Expect("terminating_deployment_uid");
  udf_tester.ForInput("badformat").Expect("");
  updates_->enqueue(px::metadatapb::testutils::CreateTerminatedDeploymentUpdatePB());
  EXPECT_OK(px::md::ApplyK8sUpdates(11, metadata_state_.get(), &md_filter_, updates_.get()));
  // make sure that we can still find the name of terminated deployment from uid
  udf_tester.ForInput("pl/terminating_deployment1").Expect("terminating_deployment_uid");
}

TEST_F(MetadataOpsTest, deployment_name_to_start_time_test) {
  auto function_ctx = std::make_unique<FunctionContext>(metadata_state_, nullptr);
  auto udf_tester =
      px::carnot::udf::UDFTester<DeploymentNameToStartTimeUDF>(std::move(function_ctx));
  udf_tester.ForInput("pl/deployment1").Expect(101);
  udf_tester.ForInput("pl/terminating_deployment1").Expect(123);
  udf_tester.ForInput("badformat").Expect(0);
  updates_->enqueue(px::metadatapb::testutils::CreateTerminatedDeploymentUpdatePB());
  EXPECT_OK(px::md::ApplyK8sUpdates(11, metadata_state_.get(), &md_filter_, updates_.get()));
  // start time shouldn't change after pod terminated
  udf_tester.ForInput("pl/terminating_deployment1").Expect(123);
}

TEST_F(MetadataOpsTest, deployment_name_to_stop_time_test) {
  auto function_ctx = std::make_unique<FunctionContext>(metadata_state_, nullptr);
  auto udf_tester =
      px::carnot::udf::UDFTester<DeploymentNameToStopTimeUDF>(std::move(function_ctx));
  udf_tester.ForInput("pl/deployment1").Expect(0);
  udf_tester.ForInput("pl/terminating_deployment1").Expect(0);
  udf_tester.ForInput("badformat").Expect(0);
  updates_->enqueue(px::metadatapb::testutils::CreateTerminatedDeploymentUpdatePB());
  EXPECT_OK(px::md::ApplyK8sUpdates(11, metadata_state_.get(), &md_filter_, updates_.get()));
  // deployment should change stop time on deployment termination
  udf_tester.ForInput("pl/terminating_deployment1").Expect(150);
}

TEST_F(MetadataOpsTest, deployment_name_to_namespace_test) {
  auto function_ctx = std::make_unique<FunctionContext>(metadata_state_, nullptr);
  auto udf_tester =
      px::carnot::udf::UDFTester<DeploymentNameToNamespaceUDF>(std::move(function_ctx));
  udf_tester.ForInput("pl/deployment1").Expect("pl");
  udf_tester.ForInput("pl/terminating_deployment1").Expect("pl");
  udf_tester.ForInput("badformat").Expect("");
  updates_->enqueue(px::metadatapb::testutils::CreateTerminatedDeploymentUpdatePB());
  EXPECT_OK(px::md::ApplyK8sUpdates(11, metadata_state_.get(), &md_filter_, updates_.get()));
  // upid2 previously was connected to 4_uid.
  udf_tester.ForInput("pl/terminating_deployment1").Expect("pl");
}

TEST_F(MetadataOpsTest, deployment_name_to_status_test) {
  auto function_ctx = std::make_unique<FunctionContext>(metadata_state_, nullptr);
  auto udf_tester = px::carnot::udf::UDFTester<DeploymentNameToStatusUDF>(std::move(function_ctx));

  // can't test with more conditions since conditions are stored in unordered map
  // with this test suite, condition order changes, so test fail if more than one
  // condition is given
  md::DeploymentConditions conditions_ready{
      {md::DeploymentConditionType::kAvailable, md::ConditionStatus::kTrue}};

  md::DeploymentConditions conditions_terminating{
      {md::DeploymentConditionType::kAvailable, md::ConditionStatus::kFalse}};

  md::DeploymentInfo dep_info_ready("", "", "", 5, 5, 4, 3, 3, 2, 5, conditions_ready, 101, 0);
  md::DeploymentInfo dep_info_terminating("", "", "", 2, 6, 5, 3, 3, 2, 0, conditions_terminating,
                                          123, 0);
  md::DeploymentInfo dep_info_terminated("", "", "", 2, 0, 0, 0, 0, 0, 0, conditions_terminating,
                                         123, 150);

  udf_tester.ForInput("pl/deployment1").Expect(DeploymentInfoToStatus(&dep_info_ready));
  udf_tester.ForInput("pl/terminating_deployment1")
      .Expect(DeploymentInfoToStatus(&dep_info_terminating));
  udf_tester.ForInput("badformat").Expect("");
  updates_->enqueue(px::metadatapb::testutils::CreateTerminatedDeploymentUpdatePB());
  EXPECT_OK(px::md::ApplyK8sUpdates(11, metadata_state_.get(), &md_filter_, updates_.get()));
  // check status after deployment terminated
  udf_tester.ForInput("pl/terminating_deployment1")
      .Expect(DeploymentInfoToStatus(&dep_info_terminated));
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

TEST_F(MetadataOpsTest, in_value_or_array_test) {
  auto function_ctx = std::make_unique<FunctionContext>(metadata_state_, nullptr);
  auto udf_tester = px::carnot::udf::UDFTester<HasValueUDF>(std::move(function_ctx));
  udf_tester.ForInput("1", "1").Expect(true);
  udf_tester.ForInput("2", "1").Expect(false);
  udf_tester.ForInput("[\"3\", \"4\"]", "4").Expect(true);
  udf_tester.ForInput("[\"4\"]", "4").Expect(true);
  udf_tester.ForInput("[\"3\", \"4\"]", "5").Expect(false);
  udf_tester.ForInput("[]", "4").Expect(false);
}

TEST_F(MetadataOpsTest, vizier_id_test) {
  auto function_ctx = std::make_unique<FunctionContext>(metadata_state_, nullptr);
  auto udf_tester = px::carnot::udf::UDFTester<VizierIDUDF>(std::move(function_ctx));
  udf_tester.ForInput().Expect(vizier_id_.str());
}

TEST_F(MetadataOpsTest, empty_vizier_id_test) {
  auto metadata_state = std::make_shared<px::md::AgentMetadataState>(
      /* hostname */ "myhost",
      /* asid */ 1, /* pid */ 123, agent_id_, "mypod", sole::uuid(), "myvizier",
      "myviziernamespace", time_system_.get());
  auto function_ctx = std::make_unique<FunctionContext>(metadata_state, nullptr);
  auto udf_tester = px::carnot::udf::UDFTester<VizierIDUDF>(std::move(function_ctx));
  udf_tester.ForInput().Expect("00000000-0000-0000-0000-000000000000");
}

TEST_F(MetadataOpsTest, vizier_name_test) {
  auto function_ctx = std::make_unique<FunctionContext>(metadata_state_, nullptr);
  auto udf_tester = px::carnot::udf::UDFTester<VizierNameUDF>(std::move(function_ctx));
  udf_tester.ForInput().Expect("myvizier");
}

TEST_F(MetadataOpsTest, vizier_namespace_test) {
  auto function_ctx = std::make_unique<FunctionContext>(metadata_state_, nullptr);
  auto udf_tester = px::carnot::udf::UDFTester<VizierNamespaceUDF>(std::move(function_ctx));
  udf_tester.ForInput().Expect("myviziernamespace");
}

TEST_F(MetadataOpsTest, basic_upid_test) {
  auto function_ctx = std::make_unique<FunctionContext>(metadata_state_, nullptr);
  auto udf_tester = px::carnot::udf::UDFTester<CreateUPIDUDF>(std::move(function_ctx));
  udf_tester.ForInput(100, 23123).Expect(md::UPID(1, 100, 23123).value());
}

TEST_F(MetadataOpsTest, basic_upid_with_asid_test) {
  auto function_ctx = std::make_unique<FunctionContext>(metadata_state_, nullptr);
  auto udf_tester = px::carnot::udf::UDFTester<CreateUPIDWithASIDUDF>(std::move(function_ctx));
  udf_tester.ForInput(2, 100, 23123).Expect(md::UPID(2, 100, 23123).value());
}

TEST_F(MetadataOpsTest, get_cidrs) {
  auto metadata_state = metadata_state_->CloneToShared();
  px::CIDRBlock pod_cidr;
  std::string pod_cidr_str("10.0.0.20/32");
  ASSERT_OK(px::ParseCIDRBlock(pod_cidr_str, &pod_cidr));
  metadata_state->k8s_metadata_state()->set_pod_cidrs({pod_cidr});
  px::CIDRBlock service_cidr;
  std::string service_cidr_str("10.1.40.0/24");
  ASSERT_OK(px::ParseCIDRBlock(service_cidr_str, &service_cidr));
  metadata_state->k8s_metadata_state()->set_service_cidr(service_cidr);

  auto function_ctx = std::make_unique<FunctionContext>(metadata_state, nullptr);
  auto udf_tester = px::carnot::udf::UDFTester<GetClusterCIDRRangeUDF>(std::move(function_ctx));
  udf_tester.Init().ForInput().Expect(
      absl::Substitute(R"(["$0","$1","$2"])", "10.0.0.1/32", pod_cidr_str, service_cidr_str));
}

TEST_F(MetadataOpsTest, namespace_name_to_namespace_id_test) {
  auto function_ctx = std::make_unique<FunctionContext>(metadata_state_, nullptr);
  auto udf_tester =
      px::carnot::udf::UDFTester<NamespaceNameToNamespaceIDUDF>(std::move(function_ctx));
  udf_tester.ForInput("namespace1").Expect("namespace_uid");
  udf_tester.ForInput("terminating_namespace1").Expect("terminating_namespace_uid");
  udf_tester.ForInput("badformat").Expect("");
  updates_->enqueue(px::metadatapb::testutils::CreateTerminatedNamespaceUpdatePB());
  // keep information about the namespace after termination
  EXPECT_OK(px::md::ApplyK8sUpdates(11, metadata_state_.get(), &md_filter_, updates_.get()));
  udf_tester.ForInput("terminating_namespace1").Expect("terminating_namespace_uid");
}

}  // namespace metadata
}  // namespace funcs
}  // namespace carnot
}  // namespace px
