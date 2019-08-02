#include <gtest/gtest.h>
#include <memory>
#include <numeric>
#include <type_traits>
#include <utility>
#include <vector>

#include "src/carnot/funcs/metadata/metadata_ops.h"
#include "src/carnot/udf/test_utils.h"
#include "src/common/base/base.h"
#include "src/shared/k8s/metadatapb/test_proto.h"
#include "src/shared/metadata/base_types.h"
#include "src/shared/metadata/pids.h"
#include "src/shared/metadata/state_manager.h"

PL_SUPPRESS_WARNINGS_START()
// NOLINTNEXTLINE(build/include_subdir)
#include "blockingconcurrentqueue.h"
PL_SUPPRESS_WARNINGS_END()

namespace pl {
namespace carnot {
namespace funcs {
namespace metadata {

using ResourceUpdate = pl::shared::k8s::metadatapb::ResourceUpdate;

class MetadataOpsTest : public ::testing::Test {
 protected:
  void SetUp() override {
    metadata_state_ = std::make_shared<pl::md::AgentMetadataState>(1);

    // Apply updates to metadata state.
    updates_ =
        std::make_unique<moodycamel::BlockingConcurrentQueue<std::unique_ptr<ResourceUpdate>>>();

    updates_->enqueue(pl::metadatapb::testutils::CreateRunningContainerUpdatePB());
    updates_->enqueue(pl::metadatapb::testutils::CreateRunningPodUpdatePB());
    updates_->enqueue(pl::metadatapb::testutils::CreateTerminatingContainerUpdatePB());
    updates_->enqueue(pl::metadatapb::testutils::CreateTerminatingPodUpdatePB());

    auto s = pl::md::AgentMetadataStateManager::ApplyK8sUpdates(10, metadata_state_.get(),
                                                                updates_.get());

    // Apply PID updates to metadata state.
    auto upid1 = md::UPID(123, 567, 89101);
    auto pid1 = std::make_unique<md::PIDInfo>(upid1, "test", "pod1_container_1");
    metadata_state_->AddUPID(upid1, std::move(pid1));
    auto upid2 = md::UPID(123, 567, 468);
    auto pid2 = std::make_unique<md::PIDInfo>(upid2, "cmdline", "pod2_container_1");
    metadata_state_->AddUPID(upid2, std::move(pid2));

    ASSERT_OK(s);
  }

  std::shared_ptr<pl::md::AgentMetadataState> metadata_state_;
  std::unique_ptr<moodycamel::BlockingConcurrentQueue<std::unique_ptr<ResourceUpdate>>> updates_;
};

TEST_F(MetadataOpsTest, asid_test) {
  auto function_ctx = std::make_unique<FunctionContext>(metadata_state_);
  auto udf_tester = pl::carnot::udf::UDFTester<ASIDUDF>(std::move(function_ctx));
  udf_tester.ForInput().Expect(1);
}

TEST_F(MetadataOpsTest, pod_id_to_pod_name_test) {
  auto function_ctx = std::make_unique<FunctionContext>(metadata_state_);
  auto udf_tester = pl::carnot::udf::UDFTester<PodIDToPodNameUDF>(std::move(function_ctx));
  udf_tester.ForInput("1_uid").Expect("pl/running_pod");
  udf_tester.ForInput("2_uid").Expect("pl/terminating_pod");
}

TEST_F(MetadataOpsTest, pod_name_to_pod_id_test) {
  auto function_ctx = std::make_unique<FunctionContext>(metadata_state_);
  auto udf_tester = pl::carnot::udf::UDFTester<PodNameToPodIDUDF>(std::move(function_ctx));
  udf_tester.ForInput("pl/running_pod").Expect("1_uid");
  udf_tester.ForInput("pl/terminating_pod").Expect("2_uid");
}

TEST_F(MetadataOpsTest, upid_to_pod_id_test) {
  auto function_ctx = std::make_unique<FunctionContext>(metadata_state_);
  auto udf_tester = pl::carnot::udf::UDFTester<UPIDToPodIDUDF>(std::move(function_ctx));
  auto upid1 = types::UInt128Value(528280977975, 89101);
  udf_tester.ForInput(upid1).Expect("1_uid");
  auto upid2 = types::UInt128Value(528280977975, 468);
  udf_tester.ForInput(upid2).Expect("2_uid");
  auto upid3 = types::UInt128Value(528280977975, 123);
  udf_tester.ForInput(upid3).Expect("");
}

TEST_F(MetadataOpsTest, upid_to_pod_name_test) {
  auto function_ctx = std::make_unique<FunctionContext>(metadata_state_);
  auto udf_tester = pl::carnot::udf::UDFTester<UPIDToPodNameUDF>(std::move(function_ctx));
  auto upid1 = types::UInt128Value(528280977975, 89101);
  udf_tester.ForInput(upid1).Expect("pl/running_pod");
  auto upid2 = types::UInt128Value(528280977975, 468);
  udf_tester.ForInput(upid2).Expect("pl/terminating_pod");
  auto upid3 = types::UInt128Value(528280977975, 123);
  udf_tester.ForInput(upid3).Expect("");
}

TEST_F(MetadataOpsTest, upid_to_container_id_test) {
  auto function_ctx = std::make_unique<FunctionContext>(metadata_state_);
  auto udf_tester = pl::carnot::udf::UDFTester<UPIDToContainerIDUDF>(std::move(function_ctx));
  auto upid1 = types::UInt128Value(528280977975, 89101);
  udf_tester.ForInput(upid1).Expect("pod1_container_1");
  auto upid2 = types::UInt128Value(528280977975, 468);
  udf_tester.ForInput(upid2).Expect("pod2_container_1");
  auto upid3 = types::UInt128Value(528280977975, 123);
  udf_tester.ForInput(upid3).Expect("");
}

}  // namespace metadata
}  // namespace funcs
}  // namespace carnot
}  // namespace pl
